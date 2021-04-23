##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

'''
IoT Event Analytics Anomaly Detection

'''
import copy
import os
import pickle

import numpy as np
import pandas as pd
import tensorflow as tf
import tensorflow_probability as tfp
from keras import activations
from keras import backend as K
from keras import initializers, optimizers
from keras.layers import Conv2D, Conv2DTranspose, Input, Layer
from keras.layers.core import (Activation, Dense, Dropout, Flatten, Lambda,
                               Reshape)
from keras.models import Model, Sequential
from keras.optimizers import SGD, RMSprop
from numpy import corrcoef
from scipy import ndimage
from scipy.spatial import distance
from scipy.stats import f_oneway, normaltest, pearsonr, ttest_ind
from sklearn.gaussian_process import GaussianProcessRegressor
from sklearn.gaussian_process.kernels import RBF, ConstantKernel
from sklearn.metrics import mean_squared_error
from sklearn.naive_bayes import GaussianNB
from statsmodels.tsa.stattools import adfuller
from tensorflow.keras import initializers, regularizers
from tensorflow.keras.callbacks import ModelCheckpoint, TensorBoard


'''
Defintions:
- Preprocessing like normalization, scaling etc. is not made by this library
- Multiple learning calls perform a transfer learning and continously learn an existing model. Argument forcelearning must be True
- The input is always a matrix, each vector represents the dimensionality, multiple vectors represent samples
- The implementation class name denotes the procedure


Binning             or <blank> no binning
Multivariate        as allowed dimenions
Series              if multiple observations are considered e.g. something over time
Classname           Spectral (aka Reconstruction)
                    Probabilistic
                    Distance
                    Classification
                    Information Theory (aka hypothesis testing)
Modelname           e.g. GaussianProcess

- Confidence is dteermined by an hypothesis test
'''

class IoTeaAnomalyDetection:

    '''
        Scoring evaluates the anomaly.
    '''
    SCORING_MSE = 'MSE'
    SCORING_DISTRIBUTION = 'DIS'
    SCORING_CORRELATION_COEFFICIENT = 'COV'
    SCORING_THREADHOLD_DEVIATION = 'THD'


    def learn(self, Features, Targets):
        pass

    def predict(self, Features):
        pass

    def checkAnomaly(self, Features, Targets):
        pass


class MultivariateSeriesProbabilisticGaussianProcess(IoTeaAnomalyDetection):
    def __init__(self,name='optimizerbfmodel',noise=0.1, forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_MSE):
        self.name = './'+name+'.h5'
        self.noise = noise
        self.forcelearning = forcelearning
        self.model = None
        self.threshold = threshold
        self.scoring = scoring

    def learn(self, Features, Targets):
        model = None
        if os.path.isfile(self.name) and self.forcelearning == False:
            # Restore the weights
            with open(self.name, 'rb') as f:
                model = pickle.load(f)
        else:
            # IoT Event Analytics normalizes the space between 0..1
            rbf = ConstantKernel(1.0) * RBF(length_scale=1.0)

            model = GaussianProcessRegressor(kernel=rbf, alpha=self.noise**2)
            model.fit(Features, Targets)
            with open(self.name,'wb') as f:
                pickle.dump(model,f)

        self.model = model

    def predict(self, Features):
        if not self.model:
            if os.path.isfile(self.name) :
                # Restore the weights
                with open(self.name, 'rb') as f:
                    self.model = pickle.load(f)
            else:
                raise Exception("Model not learned")

        return self.model.predict(Features,return_cov = True)

    def checkAnomaly(self, Features, Targets):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%
        '''
        _targets, cov= self.predict(Features)

        # Confidence check hypothesis (H0): All data are in the 96% interval
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level

        # Tolerance interval such as 1.96 for 95% coverage of the gauss population
        uncertainty = (1.96 * np.sqrt(cov))

        # MSE regression or Correlation Coefficient metrics for the loss (aka score) between given and predicted
        if self.scoring == IoTeaAnomalyDetection.SCORING_MSE:
            return mean_squared_error(_targets, Targets) >=self.threshold, 1- uncertainty
        elif self.scoring == IoTeaAnomalyDetection.SCORING_THREADHOLD_DEVIATION:
            return corrcoef(_targets, Targets) >=self.threshold, 1- uncertainty

        raise Exception("Scoring not supported")


class BinningMultivariateSeriesProbabilisticGaussianProcess(IoTeaAnomalyDetection):
    def __init__(self, name = 'binning', bins = 2):
        self.learner = []
        for i in range(0,bins):
            self.learner.append( MultivariateSeriesProbabilisticGaussianProcess(name= name+str(i)) )
        self.bins = bins
        self.name = name

    def learn(self, Features, Targets):
        self.learndata = sorted( [Features, Targets], key=lambda tup: tup[0] )
        _len = len(self.learndata[0])
        _binlenght = int(_len/ self.bins)
        self.learndata = np.array(self.learndata).reshape(2,len(Features))
        for i in range(0,self.bins):
            _xtmp = self.learndata[0,(i*_binlenght):( (i+1)*_binlenght )]
            _ytmp = self.learndata[1,(i*_binlenght):( (i+1)*_binlenght )]
            self.learner[i].learn( np.array(_xtmp).reshape(-1, 1)  , np.array(_ytmp).reshape(-1, 1) )

    def predict(self, Features):
        # IoT Event Analytics normalizes the space between 0..1
        space = np.arange(-1, 1, 0.05).reshape(-1, 1)
        position = np.argmax(space > Features)
        _binlenght = int(len(space)/ self.bins)
        _binnumber = int(position/ _binlenght)
        return self.learner[_binnumber].predict(Features)

    def checkAnomaly(self, Features, Targets=None):
        # IoT Event Analytics normalizes the space between 0..1
        space = np.arange(-1, 1, 0.05).reshape(-1, 1)
        position = np.argmax(space > Features)
        _binlenght = int(len(space)/ self.bins)
        _binnumber = int(position/ _binlenght)
        return self.learner[_binnumber].checkAnomaly(Features, Targets)


class MultivariateSeriesSpectralAutoencoder(IoTeaAnomalyDetection):

    def __init__(self, name = 'ae',forcelearning = False, input_dim = np.NaN, encoding_dim = np.NaN, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_MSE):
        '''
            A variable is stochastic if the occurrence of events or outcomes involves randomness or uncertainty.
        '''
        self.encoding_dim = encoding_dim
        self.input_dim = input_dim
        self.name = './'+name+'.pkl'
        self.forcelearning = forcelearning
        self.threshold = threshold
        self.scoring = scoring

    def predict(self, Features) :
        if not self.model:
            if os.path.isfile(self.name):
                # Restore the weights
                self.model.load_weights(self.name)
            else:
                raise Exception("Model not learned")
        return self.model.predict(Features)

    def checkAnomaly(self, Features, Targets = None):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%
        '''
        if Targets is not None:
            raise Exception("Per definition Target is equal to Features")
        _x = self.predict(Features)

        # Confidence check hypothesis (H0): No mean square error exists= identity is given
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level

        if self.scoring == IoTeaAnomalyDetection.SCORING_MSE:
            return mean_squared_error(_x, Features) >=self.threshold, 1- mean_squared_error(_x, Features)
        elif self.scoring == IoTeaAnomalyDetection.SCORING_THREADHOLD_DEVIATION:
            return corrcoef(_x, Features) >=self.threshold, 1- mean_squared_error(_x, Features)

        raise Exception("Scoring not supported")

    def learn(self, Features, Targets = None):
        num_batches = 5

        if Targets is not None:
            raise Exception("Per definition Target is equal to Features")

        if self.encoding_dim is np.NaN:
            self.encoding_dim = Features.shape[1]

        if self.input_dim is np.NaN:
            self.input_dim = (1,self.encoding_dim)

        print(self.encoding_dim,' ',self.input_dim)

        # Stupid identity function is to learn
        np.random.seed(7)
        input_layer = Input(shape=self.encoding_dim)

        encoder = Dense( self.encoding_dim, activation="tanh", activity_regularizer=regularizers.l1(10e-5))(input_layer)
        encoder = Dense( int(self.encoding_dim/2), activation="relu")(encoder)
        decoder = Dense( int(self.encoding_dim/2), activation='relu')(encoder)
        decoder = Dense( self.encoding_dim, activation='tanh')(decoder)
        model = Model(inputs=input_layer, outputs=decoder)

        model.compile(optimizer='rmsprop',
                  loss=['mse', 'categorical_crossentropy'],
                  loss_weights=[1.0, 20.0], metrics=['mean_squared_error'])
        if os.path.isfile(self.name) and self.forcelearning == False:
            # Restore the weights
            model.load_weights(self.name)
        else:
            checkpointer = ModelCheckpoint(filepath=self.name,verbose=0, save_best_only=True)
            # Autoencoders learn the identiy, due to that X = X
            model.fit(Features, Features, epochs=100, batch_size=num_batches, verbose=1,callbacks=[checkpointer])

            # Save the weights
            model.save_weights(self.name)

        self.model = model



class MultivariateSeriesDistanceCenterOfMass(IoTeaAnomalyDetection):

    def __init__(self, name = 'cof',forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_THREADHOLD_DEVIATION):
        self.name = './'+name+'.pkl'
        self.forcelearning = forcelearning
        self.threshold = threshold
        self.scoring = scoring

    def predict(self, Features) :
        raise Exception("Not implemented")

    def checkAnomaly(self, Features, Targets = None):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%
        '''
        if Targets is not None:
            raise Exception("Per definition Target is equal to Features")

        if not self.model:
            if not os.path.isfile(self.name):
                raise Exception("Model not trained")
            with open(self.name, 'rb') as f:
                self.model = pickle.load(f)

        _x = distance.euclidean(self.model, Features)

        _zero = np.zeros(self.model.shape)

        _root = distance.euclidean(_zero, Features)**2

        # Confidence check hypothesis (H0): _root/2 is in the expected space
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level
        if self.scoring != IoTeaAnomalyDetection.SCORING_THREADHOLD_DEVIATION:
            raise Exception("Scoring not supported")

        return _x >=self.threshold, _x/_root

    def learn(self, Features, Targets = None):
        if Targets is not None:
            raise Exception("Per definition Target is equal to Features")

        _cof = None

        if os.path.isfile(self.name) and self.forcelearning == False:
            with open(self.name, 'rb') as f:
                _cof = pickle.load(f)
        else:
            _rst = []
            for i in range(len(Features)):
                if i == 0:
                    _rst = Features[0]
                else:
                    _rst = _rst + Features[i]
            _cof  = _rst / len(Features)
            with open(self.name,'wb') as f:
                pickle.dump(_cof,f)

        self.model = _cof



class MultivariateSeriesClassificationGaussianNB(IoTeaAnomalyDetection):
    def __init__(self,name='gaussiannb',noise=0.1, forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_DISTRIBUTION):
        self.name = './'+name+'.h5'
        self.noise = noise
        self.forcelearning = forcelearning
        self.model = None
        self.threshold = threshold
        self.scoring = scoring

    def learn(self, Features, Targets):
        '''
            Important: Target have to be classes as integer
        '''
        model = None
        if os.path.isfile(self.name) and self.forcelearning == False:
            # Restore the weights
            with open(self.name, 'rb') as f:
                model = pickle.load(f)
        else:
            model = GaussianNB()
            model.fit(Features, Targets)
            with open(self.name,'wb') as f:
                pickle.dump(model,f)

        self.model = model

    def predict(self, Features):
        raise Exception("Not implemented")

    def checkAnomaly(self, Features, Targets):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%

            Check if a features are part of a target classes as learned.

            Assumption is that each feature is normal distributed.
        '''
        if self.scoring != IoTeaAnomalyDetection.SCORING_DISTRIBUTION:
            raise Exception("Scoring not supported")

        if not self.model:
            if not os.path.isfile(self.name):
                raise Exception("Model not trained")
            with open(self.name, 'rb') as f:
                model = pickle.load(f)

        _score = self.model.score(Features,Targets)

        # Confidence check hypothesis (H0): The given class elements are normal distributed
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level
        return _score <=self.threshold,self.model.predict_proba(Features)[0][0]



class MultivariateSeriesInformationTheoryNormalDistribution(IoTeaAnomalyDetection):
    def __init__(self,name='itdistribution',noise=0.1, forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_DISTRIBUTION):
        self.name = './'+name+'.h5'
        self.noise = noise
        self.forcelearning = forcelearning
        self.model = None
        self.threshold = threshold
        self.scoring = scoring

    def learn(self, Features, Targets):
        raise Exception("Not implemented")

    def predict(self, Features):
        raise Exception("Not implemented")

    def checkAnomaly(self, Features, Targets):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%

            Check if a features are part of a target classes as learned.

            Assumption is that each feature is normal distributed.
        '''
        if self.scoring != IoTeaAnomalyDetection.SCORING_DISTRIBUTION:
            raise Exception("Scoring not supported")
        if Targets:
            raise Exception("Targets not support")

        # Confidence check hypothesis (H0): Normal distribution is given
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level
        stat, p = normaltest(Features)

        return p <=self.threshold,stat

class MultivariateSeriesInformationTheoryCorrelationNormalPearson(IoTeaAnomalyDetection):
    def __init__(self,name='itdistribution',noise=0.1, forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_DISTRIBUTION):
        self.name = './'+name+'.h5'
        self.noise = noise
        self.forcelearning = forcelearning
        self.model = None
        self.threshold = threshold
        self.scoring = scoring

    def learn(self, Features, Targets):
        raise Exception("Not implemented")

    def predict(self, Features):
        raise Exception("Not implemented")

    def checkAnomaly(self, Features, Targets):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%

            Check if a features are part of a target classes as learned.

            Assumption is that each feature is normal distributed.
        '''
        if self.scoring != IoTeaAnomalyDetection.SCORING_CORRELATION_COEFFICIENT:
            raise Exception("Scoring not supported")

        # Confidence check hypothesis (H0):
        # - Observations in each sample are normally distributed.
        # - Observations in each sample have the same variance.
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level
        stat, p = pearsonr(Features, Targets)

        return p <=self.threshold,stat


class MultivariateSeriesInformationTheoryDistributedTTest(IoTeaAnomalyDetection):
    def __init__(self,name='itdistribution',noise=0.1, forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_DISTRIBUTION):
        self.name = './'+name+'.h5'
        self.noise = noise
        self.forcelearning = forcelearning
        self.model = None
        self.threshold = threshold
        self.scoring = scoring

    def learn(self, Features, Targets):
        raise Exception("Not implemented")

    def predict(self, Features):
        raise Exception("Not implemented")

    def checkAnomaly(self, Features, Targets):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%

            Check if a features are part of a target classes as learned.

            Assumption is that each feature is normal distributed.
        '''
        if self.scoring != IoTeaAnomalyDetection.SCORING_CORRELATION_COEFFICIENT:
            raise Exception("Scoring not supported")

        # Confidence check hypothesis (H0):
        # - Observations in each sample are normally distributed.
        # - Observations in each sample have the same variance.
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level
        stat, p = ttest_ind(Features, Targets)

        return p <=self.threshold,stat

class MultivariateSeriesInformationTheoryVarianceANOVA(IoTeaAnomalyDetection):
    def __init__(self,name='itdistribution',noise=0.1, forcelearning = False, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_DISTRIBUTION):
        self.name = './'+name+'.h5'
        self.noise = noise
        self.forcelearning = forcelearning
        self.model = None
        self.threshold = threshold
        self.scoring = scoring

    def learn(self, Features, Targets):
        raise Exception("Not implemented")

    def predict(self, Features):
        raise Exception("Not implemented")

    def checkAnomaly(self, Features, Targets):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%

            Check if a features are part of a target classes as learned.

            Assumption is that each feature is normal distributed.
        '''
        if self.scoring != IoTeaAnomalyDetection.SCORING_CORRELATION_COEFFICIENT:
            raise Exception("Scoring not supported")

        # Confidence check hypothesis (H0):
        # - Observations in each sample are normally distributed.
        # - Observations in each sample have the same variance.
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level
        stat, p = f_oneway(Features, Targets)
        return p <=self.threshold,stat

class MultivariateSeriesSpectralVarinationalAutoencoder(IoTeaAnomalyDetection):

    def __init__(self, name = 'vae',forcelearning = False, input_dim = np.NaN, encoding_dim = np.NaN, threshold = 0.1, scoring = IoTeaAnomalyDetection.SCORING_MSE):
        '''
            A variable is stochastic if the occurrence of events or outcomes involves randomness or uncertainty.
        '''
        self.encoding_dim = encoding_dim
        self.input_dim = input_dim
        self.name = './'+name+'.pkl'
        self.forcelearning = forcelearning
        self.threshold = threshold
        self.scoring = scoring

    def predict(self, Features) :
        if not self.model:
            if os.path.isfile(self.name):
                # Restore the weights
                self.model.load_weights(self.name)
            else:
                raise Exception("Model not learned")
        return self.model.predict(Features)

    def checkAnomaly(self, Features, Targets = None):
        '''
            Anomaly     True|False
            Uncertaincy 0...100%
        '''
        if Targets is not None:
            raise Exception("Per definition Target is equal to Features")
        _x = self.predict(Features)

        # Confidence check hypothesis (H0): No mean square error exists= identity is given
        # Significance level = reject or accept H0
        # Confidence level = 1 - significance level

        if self.scoring == IoTeaAnomalyDetection.SCORING_MSE:
            return abs(mean_squared_error(_x, Features)) >=self.threshold, abs(1- mean_squared_error(_x, Features) )
        elif self.scoring == IoTeaAnomalyDetection.SCORING_THREADHOLD_DEVIATION:
            return abs(corrcoef(_x, Features)) >=self.threshold, abs( 1- mean_squared_error(_x, Features) )

        raise Exception("Scoring not supported")

    def learn(self, Features, Targets = None):
        num_batches = 5

        if Targets is not None:
            raise Exception("Per definition Target is equal to Features")

        if self.encoding_dim is np.NaN:
            self.encoding_dim = Features.shape[1]

        if self.input_dim is np.NaN:
            self.input_dim = (1,self.encoding_dim)

        print(self.encoding_dim,' ',self.input_dim)

        # Stupid identity function is to learn
        np.random.seed(7)

        input_layer = Input( shape=self.encoding_dim )

        prior_sigma = []
        for i in range(Features.shape[1]):
            prior_sigma.append(np.array(Features)[0][i])

        for i in range(Features.shape[0]):
            for j in range(Features.shape[1]):
                prior_sigma[j] = tf.float64.as_numpy_dtype( np.array(Features)[i][j] - ( (prior_sigma[j] + np.array(Features)[i][j])/2 ) )


        prior_params = {
            'prior_sigma': prior_sigma,   # Standrd deviation Axis 1..n
            'prior_pi': 0.5         # Prior probability is initialized as 50%
        }
        kl_weight = 1.0 / num_batches

        encoder = DenseVariational( self.encoding_dim, kl_weight=kl_weight, **prior_params, activation="tanh")(input_layer)
        encoder = DenseVariational( int(self.encoding_dim/2), kl_weight=kl_weight, **prior_params, activation="relu")(encoder)
        decoder = DenseVariational( int(self.encoding_dim/2), kl_weight=kl_weight, **prior_params, activation='relu')(encoder)
        decoder = DenseVariational( self.encoding_dim, kl_weight=kl_weight, **prior_params, activation="tanh")(decoder)
        model = Model(inputs=input_layer, outputs=decoder)

        model.compile(optimizer='rmsprop',
                      loss=['mse', 'categorical_crossentropy'],
                      loss_weights=[1.0, 20.0], metrics=['mean_squared_error'])
        if os.path.isfile(self.name) and self.forcelearning == False:
            # Restore the weights
            model.load_weights(self.name)
        else:
            checkpointer = ModelCheckpoint(filepath=self.name,verbose=0, save_best_only=True)
            # Autoencoders learn the identiy, due to that X = X
            model.fit(Features, Features, epochs=100, batch_size=num_batches, verbose=1,callbacks=[checkpointer])

            # Save the weights
            model.save_weights(self.name)

        self.model = model

#XX = np.matrix( [[0.1,0.2,0.4,0.8,0.16,0.18]] ) # 2 Sample
#XX = np.matrix( [[-1, -1], [-2, -1], [-3, -2], [1, 1], [2, 1], [3, 2]] ) # 2 Sample
#YY = np.matrix( [[0.2],[0.4]] ) # 2 Sample
#YY = np.matrix( [[1],[1],[1],[2],[2],[2]] ) # 2 Sample
#ae = MultivariateSeriesSpectralVarinationalAutoencoder()
#ae.learn(XX)
#print( ae.checkAnomaly( [[0.1,0.2,0.4,0.8,0.16,0.18]] ) )
