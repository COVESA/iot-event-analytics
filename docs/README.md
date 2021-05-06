<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics

Bosch.IO GmbH
under [MPL-2.0 licence](https://choosealicense.com/licenses/mpl-2.0/)

## Talents

Talents encapsulate the data demand for a functional processing unit. Each Talent just has to define its required as well as offered data and services results. How a request or a reply reaches the consumer is managed by  at least one __Instance-Manager__ core talent. This enables the concept of non-blocking channeling ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/MessageChannel.html)) IO.

Each passed data follows a __request-reply__ ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/RequestReply.html)), not a request-response integration ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/EncapsulatedSynchronousIntegration.html)), communication style.

By doing so the orchestration is __fault tolerant__ and __high available__, providing full flexibility for __redundancy concepts__ e.g. easily managing off- and online situations, as well as across distributed environmental nodes e.g. across multiple embedded devices and clouds.

__Note:__ The high availability as well as supporting fault tolerance depends on the used publish subscribe implementation.

Talent examples are given for

- NodeJS here: [NodeJS SDK](../src/sdk/javascript/examples)
- Python >3.6 here [Python SDK](../src/sdk/python/examples)
- C++ here [CPP SDK](../src/sdk/cpp/examples)

### Core Components

The core components providing the platform layer on top of a publish-subscribe network protocol.

- Can be deployed in the cloud (e.g. Kubernetes, Any Node.js capable runtime, via Docker-compose)
- Can be deployed on the edge device itself --> Traffic to cloud (for cloud talents) is reduced to a minimum by using smart message routing via mosquitto

#### Config-Manager (1 or many)

Hosts the following capabilities:

- Sends out periodically discovery messages to all talents
- Validates exchanged data
- Is responsible for the feature encoding required for machine learning capabilities
- Holds the master of the __Metadata-Manager__ and updates all __Metadata-Manager worker__
- Hosts the __Rules-Manager__ which keeps the __signal temporal logic (aka STL)__

#### Instance-Manager (1 or many)

Hosts the following capabilities:

- Keeps track of all instances of a given subject
- Sets and retrieves features from and for instances
- Keeps time-to-live, value history and rolling time window for data points

#### Metadata-Manager Worker

Holds:

- Definition of Segments
- Definition of types inheriting all features from a given segment
- Inherited features can be overridden (except the index field)
- Synchronized via MQTT by Config-Manager

#### Ingestion

- Transforms and validates incoming events, configurable as ETL-Pipeline
- Multi-Channel supports an arbitrary amount of parallel ETL-Pipelines
- Completely configurable via config files (JSON Data transformations) and JSON Schema validations
- Validates value type against metadata (boolean, string, number, object, any)
- Validates type against metadata using Metadata-Manager Slave
- Freely scalable by ingestion instance count

#### Encoding

- Encodes incoming events by various encoders (minmax, delta, categorical) --> Not implemented yet
- Uses Metadata-Manager Slave to retrieve metadata
- Uses Instance-Manager to store an encoded field into the appropriate instance
- Freely scalable by ingestion instance count

#### Routing

- Freely scalable by ingestion instance count
- Uses rules to determine whether an event should be forwarded to a specific talent
  - Rules can be any type of boolean algebra (containing and/or rules)
  - Enriches event with all fields a talent is "interested" in

#### Talent

- Supports local and cloud talents (using FAAS)
  - Automatically syncs to cloud message broker via mosquitto bridging
- Encapsulates business logic
- Programming language agnostic (Only JSON processing required)
- Map-Reduce support by using Mapper, Workers and Reducer
- FunctionTalents mimic RPC based on streaming events

# Processing

## Accessing and providing data

Required data is defined by so called __signal temporal logic__ (aka STL) rules. In its simplest case represents Boolean rules of __"when the data is given"__.

- __Consume data:__ The STL rules declares the data demand via the `getRules` interface. If the STL rule is validated to __true__ the method  `onEvent` is triggered.
- __Offer data:__  The offered data is populated via the `addOutput` method. It is not intended that the program directly passes data to topics. Due to the event driven approach the __output__ is __only__ a consequence of the `onEvent` method return.

Example without encoding:

```Javascript
this.addOutput(this.output, {
    description: 'This is the property '+this.output,
    history: 10,
    encoding: {
        type: iotea.constants.ENCODING_TYPE_STRING,
        encoder: null
    },
    unit: {
        fac: 1,
        unit: 'customunit',
        desc: 'custom unit'
    }
});
```

## Accessing and providing services

As data points services are ammped via __publish subscribe topics__ and managed via the __core talents__.

- __Consume services:__ The consumed services are to denote via the `callees` interface.
- __Offer services:__  The offered service is populated via the `registerFunction` method.

## Common definitions & plain integration

- Data points and services are managed in so called namespaces. Namespaces are prefixes or roots of __publish subscribe topics__. __STL rules__ do __not__ have access to namespaces.
- Platform messages follow fixed [JSON schema](../resources/).

Example message:

```json
{
  "returnTopic": "iotea/ingestion/events",
  "$features": {
    "default": { "instance.feature": {} }
  },
  "subject": "default",
  "type": "default",
  "instance": "default",
  "feature": "instance.feature",
  "value": "1609305408279",
  "whenMs": 1609305408279,
  "now": 1609305408299,
  "msgType": 1,
  "$metadata": {
    "description": "This is the timestamp",
    "history": 10,
    "encoding": { "type": "string", "encoder": null },
    "unit": "customunit",
    "idx": 8,
    "$unit": { "fac": 1, "unit": "customunit", "desc": "custom unit" }
  },
  "segment": "000000",
  "cid": "6c5e60ac-440a-4e5e-9121-bf7dedabcc75",
  "$feature": {
    "whenMs": 1609305408279,
    "ttlMs": 1609305438279,
    "history": [ {}, {}, {} ],
    "raw": "1609305408279",
    "enc": null,
    "stat": null
  }
}
```

- The topics follow a generic naming schema, as follows

Data distribution:

- /NAMESPACE/($share/)__(remote/)talent/NAME/events__ for passing messages into the agent network
- /NAMESPACE/($share/)__(remote/)talent/NAME/events/+__ for distributed service calls
- /NAMESPACE/($share/)__(remote/)talent/NAME/events/\<callChannelId\>/+__ for "private" communication channel between talent instance. __callChannelId__ as random UUIDv4
- /NAMESPACE/($share/)TALENTID/__configManager/talents/discover__ for talent discovering purposes
- /NAMESPACE/__discover__ for talent discovering purposes
- /NAMESPACE/__routing__ for talent discovering purposes
- /NAMESPACE/__platform__ for internal events

Data ingestion:

- /NAMESPACE/__ingestion__ for data coming from outside the agent network and may not following data schema
- /NAMESPACE/__encoding__

Core talents:

- /NAMESPACE/__metaDataManager__
- /NAMESPACE/__configManager__
- /NAMESPACE/__rulesManager__
- /NAMESPACE/__instanceManager__

__Note:__ The default namespace is per definition __iotea__.

# Machine Learning Support

The foundation of the machine learning approaches is the given matrix/ tensor encoding. With a given matrix it is quite easy to determine the identity. Identity is one core pillar of machine learning, because it help to determine the distance and due to that do respective optimization steps (loss or gain based).

## Meta Data Model

![Image of IoTea](./assets/metamodelEncoding.png)

- __Subjects__ (Instance-level), e.g. a specific vehicle with unique VIN, relate to __Segments__ (Class-level)
- __Subjects__ (Instance-level) contain __Types__ (Class-level) e.g. e.g. steering, braking, powertrain, infotainment for vehicle ECUs
- __Segments__(Class-level), e.g. a room, a plant [component], a vehicle [ECU], contain __Instances__
- __Types__ (Class-level) relate to __Instances__(Instance-level) , e.g. braking ECU with serial number XYZ, providing __Features__ (aka datapoints and services)

__Note:__ __Segments__ and __types__ can define features. A given __type__ inherits all features from the __segment__ it belongs to (Inheritance). An __instance__ is an instance of a given type, having all features from the __type__ and its __segment__.

## Data Ingestion

Basically each data point can define an so called __encoding__. Encoding data is about bringing data into the __meta data model__ e.g. translating a given textual sentence into a numerical vector space. For example compare the following [example](..\..\src\sdk\javascript\examples\integrations\vapp\config\types.json).

The encoding implicitly takes places via the core talents, retrieving the configuration on startup via

- the static respective config file is the __types.json__. For example compare the [VAPP example](..\..\src\sdk\javascript\examples\integrations\vapp\config\types.json) and,
- the dynamic configuration of each ```addOutput``` method allowing the following encoding types
- leave empty for disabling the encoding
- __through__ for pre-encoded values in the range [0..1]
- __minmax__ for numeric values having a minimum and maximum value range e.g. temperature, speed
- __delta__ for continuously rising or falling numeric values e.g. time, total energy consumption
- __categoric__ for categorical values given in a enumeration of e.g. string values, boolean values, complex object values

Besides the encoding the __ingestion__ follows the Extract-Transform-Load (aka ETL) flow. These ETL flows are called __channel__. An good example is given for the [Eclipse Ditto input format](..\..\src\sdk\javascript\examples\basic\config\channels). If the __channel__ are not sufficient, the given __adapter__ concept supports to write transformations which require specific processing capabilities, compare [the VSS adapter](..\..\src\tools\vss).

## Data Selection

Data is sourced via the data points and services. The __STL rule__ definition allow to select demanded data. The given data follows an __event sourcing__ retrieval semantic. If e.g. three data points are independently requested and an Boolean-OR STL rule, every single datapoint update passed the updated and the the other two last known states.

Each __STL rule__ consists of a Boolean expression (AND, OR) orchestrating __constraints__. Every __constraint__ filters on Class-level by

- __type__ and,
- __feature__ selector e.g.:

```javascript
new Rule(
  new OpConstraint(<feature>, OpConstraint.OPS.GREATER_THAN, 0, <type>, VALUE_TYPE_RAW)
)
```

### Selector

Per default the __type__ and __feature__ can be the __name__ or the wildcard __*__.

### Constraints

Constraints offer __operations__ performing the validation to __true__ or __false__,

- OpConstraint gets trigger when the condition evaluates to __true__
  - ISSET
  - EQUALS
  - NEQUALS
  - LESS_THAN
  - LESS_THAN_EQUAL
  - GREATER_THAN
  - GREATER_THAN_EQUAL
  - REGEX
- ChangeConstraint gets trigger when the referenced feature __changes__
- NelsonConstraint gets trigger when the condition evaluates to __true__
  - ALTER
  - BIAS
  - HIGH_DEV
  - LOW_DEV
  - OUT1_SE
  - OUT2_SE
  - OUT3_SE
  - TREND
- TimeseriesPatternConstraint gets trigger when the history somewhere contains a specified __data pattern__. The pattern is described via a JSON array
  including so called __wildcards__. A wildcard is equal to the regular expression statement __.*__, but can contain any (complex) object definition. In addition four methods support the wildcard definition flexibility
  - .accept( OBJECT )
  - .reject( OBJECT )
  - .minValues( OBJECTS )
  - .maxValues( OBJECTS )

## Data access

Each __feature__ keeps five values

- The raw value via the __feature.raw__ attribute
- The encoded value via the __feature.enc__ attribute
- Metadata like unit of measurement via the __features.\<type\>.\<feature\>.metadata__ attribute
- The history of all previous values via the __feature.history__ attribute
- Statistic measures via the __feature.stat__ attribute

## Parallel Data Processing

Via the Map-Reduce programming model the logic is executed on the server where the data, __may already__, resides and where the massive computation performance exists.

- __Mapping__ is the task of splitting work to __worker__
- __Worker__ performs the required processing/calulations
- __Reducer__ that the __workers__ result and combines it into a "single" result

Three respective classes encapsulate the required agent network communication:

- `Mapper` with two interface `getTriggerRules` and `map`. The method map results the data array for the workers.
- `Worker` with the interface `work`. Argument is per instance one portion of mapper's data.
- `Reducer` with the interface `reduce` Argument is per instance one portion of worker's data.

__Note:__ The C++ implementation currently not support this feature.

## Anomaly Detection

Anomaly detection is about knowing the __value identity of a given variable to prior variables__ (aka datapoints, feature).

Mathematically the following classes are provided:

- __Statistical approaches__ which takes multiple observations of expected observations and the probability (distribution, aka probability density function) of observations into account.
- __Distance approaches__ which maps the objective into a vector (space) and take norms into account.
- __Classification approaches__ which extends the considered space by finding regressions, forming cluster by comparing the value with the regressor. It is the the (statistical) dependence between two or more variables (aka “mutual information”).
- __Spectral approaches__ which performs a dimension reduction, determining the latent (aka hidden) space, describing and may reconstructing the higher dimensional space.
- __Information theory approaches__ in contrast to the __statistical approaches__ it determines the uncertainty (aka __relative*__ entropy) in comparison to hypothesis. Entropy quantifies how much information in the variables (event) probability distribution.

Independent from the applied detection class, the __anomaly quantification__ (aka scoring) of the identity deviation plays a central role. The quantification happens via respective hypothesis testing.

The root interface below _../src/sdk/python/extensions/_ is:

```python
class IoTeaAnomalyDetection:
    def learn(self, Features, Targets):
        pass

    def predict(self, Features):
        pass

    def checkAnomaly(self, Features, Targets):
        pass
```

The method `checkAnomaly` returns a boolean value if an anomaly exists and the confidence factor.

The amount of considered variables play a central role for applicable detection class:

- __Univariate__ analyses only one variable. No cause or relationship is considered. Most commonly it is done variable
  central tendency measures like the expected value (aka mean) and spread considerations like variance.

  Per default each numeric variable has statistical information of the
  - event count,
  - mean value,
  - variance as well as,
  - standard deviation
  given.

  Besides these statistical measures the signal temporal logic (aka STL) rules allow to define Nelson rules, describing statistical pattern. Temporal logic is a kind of qualifying data points terms of __time__ - temporal logic has the ability reasoning a timeline. Programatically it is described by the __talent's__ `getRules` method.

- For __Univariate__ and __Multivariate__ involves one and more variables, dealing with causes and relationships like the correlation and cluster
  estimates.

  Some basic definition:
  - Preprocessing like normalization, scaling etc. is not made by this library
  - Multiple learning calls perform a transfer learning and continuously learn an existing model
  - The input is always a matrix, each vector represents the dimensionality, multiple vectors represent samples
  - The implementation class name denotes the procedure
    - __Binning__  or __blank__ no binning
    - Allowed dimensions, __Univariate__ or __Multivariate__
    - __Series__ if multiple observations are considered e.g. something over time or __blank__
    - __Type__
        a. Spectral (aka Reconstruction)
        b. Probabilistic (aka stochastic)
        c. Distance (aka dimensional reduction)
        d. Classification (aka dimensional extension)
        e. InformationTheory (aka hypothesis testing)
    - __Modelname__ e.g. GaussianProcess

  Given are the following implementations:
  - MultivariateSeriesProbabilisticGaussianProcess. Decide if the correlation of an observation fits to the known one under correlation assumption.
  - BinningMultivariateSeriesProbabilisticGaussianProcess. As MultivariateSeriesProbabilisticGaussianProcess but with multiple folds.
  - MultivariateSeriesSpectralAutoencoder. Decide if the describing feature are stable for an given observation to the known behaviour.
  - MultivariateSeriesDistanceCenterOfMass. Decide how close the mass of an observation is to the known.
  - MultivariateSeriesClassificationGaussianNB. Decide how good does the input - output correlation match to the known one.
  - MultivariateSeriesInformationTheoryNormalDistribution. Decide is the underlying process is normal distributed.
  - MultivariateSeriesInformationTheoryCorrelationNormalPearson. Decide if two observations fit to another.
  - MultivariateSeriesInformationTheoryDistributedTTest. Decide if two observations are normal distributed, in its fitting.
  - MultivariateSeriesInformationTheoryVarianceANOVA. Decide how equal are the variances of two observations.

# Build And Deployment Components

IoT Event Analytics primarily consists of 3 components, except the extensions like adapter:

- __Pipeline__, containing (exemplary configuration can be found [here named "pipeline"](../docker/pipeline/),
  - __Ingestion__,
  - __Encoding__ and,
  - __Routing__ capabilities.
- __Platform__, containing (exemplary configuration can be found [here name "platform"](../docker/platform/),
  - __Configuration-Manager__ (exemplary configuration can be found [here named "config"](../docker/config/),
  - __MetaData-Manager__ and,
  - __Instance-Manager__. Often the __Configuration Manager__ is separately deployed, because of the Restful web-service API for __MetaData-Management API__ and __Instance-Management API__.
- __Publish-Subscribe system__ which is per default Eclipse Mosquitto (exemplary configuration can be found [here named "mosquitto"](../docker/config/) ). For details on Eclipse Mosquitto please consult the respective Eclipse project documentation.

Generally each component configuration consists of a _config.json_ defining:

- The logging level
- The platform ID separating tenants
- Broker configuration (wrongly) denoted via a __mqtt__ section
  - A connecting string
  - A namespace (short ns)
- For the Configuration-Manager also
  - a talent discovery interval
  - a web service API definition, mainly the port
- for the platform also

  - the statically defined meta-model including predefined features (aka datapoints), denoted via the file ```types.json```. The typing defines how the instance behave. Each __segment__ (e.g. "100000" as unique number) of the meta-model contains specializations, so called __types__. The segment defines __features__ (aka data points) for all related __Types__. Each __Type__ adapt and extend this definition.<br>
  __Example:__

    ```json
    {
      "100000": {
          "features": {
              "anyfeature": {
                  "idx": 0,
                  "encoding": {
                      "type": "number",
                      "encoder": "minmax",
                      "min": 0,
                      "max": 20
                  },
                  "description": "Any feature in the range of [0..20]",
                  "unit": "°C"
              }
          },
          "types": {
              "anytype": {
                  "features": {}
              }
          }
      }
    }
    ```

  - the statistically defined unit of measurements, denoted via the file ```uom.json```. The unit of measurement definition forms a tree of conversion factors.<br>
    Example:

    ```json
      "ONE": {
          "fac": 1,
          "unit": "one",
          "desc": "Eins"
      },
      "%": {
          "fac": 0.01,
          "unit": "%",
          "desc": "percent",
          "ref": "ONE"
      },
    ```

The Build is performed via Docker (Compose). The configuration files for

- Docker can be found [here](../docker/) and,
- Docker-Compose [here](../docker-compose/).

# Tooling

The following tools support the development and integration:

- All JSON Schema can be found [here](..\src\resources\).
- Node-Red input and output node: [here](..\src\sdk\node-red\).
- Microsoft Visual Studio Code Plugin: [here](..\src\sdk\vscode\).
- Message publishing e.g. used for passing the event, located in the events.txt files, parallel to each example: [here](..\src\tools\mqtt).
- VSS tooling for converting VSS JSON files into IoT Event Analytics types and features: [here](..\src\tools\vss).
- Adapter
  - DAPR integration for consuming data points and calling services: [here](..\src\adapter\dapr).
  - VSS integration for bidirectional data passing with Eclipse KUKSA.VAL: [here](..\src\adapter\vss).

# Additional Documentation

Additional documentation can be found below the topics folder:
- [Communication](./topics/iotea-communication.md)
- [Components](./topics/iotea-components.md)
- [Talents](./topics/iotea-talents.md)
  - [Feature Engineering](./topics/iotea-feature-engineering.md)
- [Machine Learning](./topics/machine-learning.md)
- [Internals](./topics/iotea-internals.md)
- [Write a Talent with just MQTT](./topics/iotea-writing-a-talent-with-just-mqtt.md)
