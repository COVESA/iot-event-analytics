/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const {
    VALUE_TYPE_ENCODED
} = require('./constants');

const {
    Constraint,
    PATH_IDENTITY
} = require('./rules');

const TimeseriesConstraint = require('./rules.tseries').TimeseriesConstraint;
class NelsonConstraint extends TimeseriesConstraint {
    constructor(feature, nelsonType, typeSelector, minValueCount, instanceIdFilter, limitFeatureSelection) {
        // Path only supports PATH_IDENTITY, since Nelson constraints only work on encoded values, which are always plain numbers
        super(feature, Constraint.OPS.NELSON, nelsonType, typeSelector, VALUE_TYPE_ENCODED, minValueCount, PATH_IDENTITY, instanceIdFilter, limitFeatureSelection);
    }
}

NelsonConstraint.fromJson = function fromJson(constraint) {
    if (constraint.op !== Constraint.OPS.NELSON) {
        throw new Error(`Invalid operation found for constraint. Expected ${Constraint.OPS.NELSON}, got ${constraint.op}`);
    }

    switch(constraint.value) {
        case NelsonConstraint.NELSON_TYPE.ALTER: {
            return new NelsonAlterConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.BIAS: {
            return new NelsonBiasConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.HIGH_DEV: {
            return new NelsonHighDevConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.LOW_DEV: {
            return new NelsonLowDevConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.OUT1_SE: {
            return new NelsonOut1SeConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.OUT2_SE: {
            return new NelsonOut2SeConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.OUT3_SE: {
            return new NelsonOut3SeConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
        case NelsonConstraint.NELSON_TYPE.TREND: {
            return new NelsonTrendConstraint(constraint.feature, constraint.typeSelector, constraint.instanceIdFilter, constraint.limitFeatureSelection);
        }
    }

    throw new Error(`Invalid Nelson type ${constraint.value} given`);
};

NelsonConstraint.NELSON_TYPE = {
    OUT3_SE: 0,
    OUT2_SE: 1,
    OUT1_SE: 2,
    BIAS: 3,
    TREND: 4,
    ALTER: 5,
    LOW_DEV: 6,
    HIGH_DEV: 7
}

class NelsonAlterConstraint extends NelsonConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.ALTER, typeSelector, 14, instanceIdFilter, limitFeatureSelection);
    }

    __evaluate(values, stat) {
        let successiveValueCount = 1;
        let isHigh = values[0] > stat.mean;

        for (let i = 1; i < values.length - 1; i++) {
            if (isHigh && values[i] < stat.mean) {
                successiveValueCount++;
                isHigh = false;
                continue;
            }

            if (!isHigh && values[i] > stat.mean) {
                successiveValueCount++;
                isHigh = true;
                continue;
            }

            successiveValueCount = 0;
            isHigh = values[i] > stat.mean;
        }

        return successiveValueCount >= this.minValueCount;
    }
}

class NelsonTrendConstraint extends NelsonConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.TREND, typeSelector, 6, instanceIdFilter, limitFeatureSelection);
    }

    __evaluate(values) {
        let successiveValueCount = 0;

        for (let i = 0; i < values.length - 1; i++) {
            if (values[i] > values[i + 1]) {
                // Value is Rising, since values at beginning of array are most recent
                successiveValueCount = successiveValueCount < 0 ? 1 : successiveValueCount + 1;
            }

            if (values[i] < values[i + 1]) {
                // Value is Falling, since values at beginning of array are most recent
                successiveValueCount = successiveValueCount > 0 ? -1 : successiveValueCount - 1;
            }

            if (Math.abs(successiveValueCount) === this.minValueCount) {
                return true;
            }
        }

        return false;
    }
}

class NelsonBiasConstraint extends NelsonConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.BIAS, typeSelector, 9, instanceIdFilter, limitFeatureSelection);
    }

    __evaluate(values, stat) {
        let successiveValueCount = 0;

        for (let i = 0; i < values.length; i++) {
            if (values[i] > stat.mean) {
                successiveValueCount = successiveValueCount < 0 ? 1 : successiveValueCount + 1;
            }

            if (values[i] < stat.mean) {
                successiveValueCount = successiveValueCount > 0 ? -1 : successiveValueCount - 1;
            }

            if (Math.abs(successiveValueCount) === this.minValueCount) {
                return true;
            }
        }

        return false;
    }
}

class NelsonDevConstraint extends NelsonConstraint {
    constructor(feature, nelsonType, typeSelector, minValueCount, checkHighDeviation, instanceIdFilter, limitFeatureSelection) {
        super(feature, nelsonType, typeSelector, minValueCount, instanceIdFilter, limitFeatureSelection);
        this.checkHighDeviation = checkHighDeviation;
    }

    __evaluate(values, stat) {
        const upperThreshold = stat.mean + stat.sdev;
        const lowerThreshold = stat.mean - stat.sdev;

        let successiveValueCount = 0;

        for (let i = 0; i < values.length; i++) {
            if (values[i] === upperThreshold || values[i] === lowerThreshold) {
                successiveValueCount++;
            } else {
                if (this.checkHighDeviation && (values[i] > upperThreshold || values[i] < lowerThreshold)) {
                    successiveValueCount++;
                } else if (!this.checkHighDeviation && (values[i] < upperThreshold && values[i] > lowerThreshold)) {
                    successiveValueCount++;
                } else {
                    successiveValueCount = 0;
                }
            }

            if (successiveValueCount >= this.minValueCount) {
                // Break early
                return true;
            }
        }

        return successiveValueCount >= this.minValueCount;
    }
}

class NelsonHighDevConstraint extends NelsonDevConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.HIGH_DEV, typeSelector, 8, true, instanceIdFilter, limitFeatureSelection);
    }
}

class NelsonLowDevConstraint extends NelsonDevConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.LOW_DEV, typeSelector, 15, false, instanceIdFilter, limitFeatureSelection);
    }
}

class NelsonOutSeConstraint extends NelsonConstraint {
    constructor(feature, nelsonType, typeSelector, minValueCount, deviationFactor, minExceedCount, instanceIdFilter, limitFeatureSelection) {
        super(feature, nelsonType, typeSelector, minValueCount, instanceIdFilter, limitFeatureSelection);
        this.deviationFactor = deviationFactor;
        this.minExceedCount = minExceedCount;
    }

    __evaluate(values, stat) {
        const upperThreshold = stat.mean + this.deviationFactor * stat.sdev;
        const lowerThreshold = stat.mean - this.deviationFactor * stat.sdev;

        let exceedsUpper = 0;
        let exceedsLower = 0;

        for (let i = 0; i < values.length; i++) {
            if (values[i] > upperThreshold) {
                exceedsUpper++;
            }

            if (values[i] < lowerThreshold) {
                exceedsLower++;
            }

            if (i >= this.minValueCount) {
                if (values[i - this.minValueCount] > upperThreshold) {
                    exceedsUpper--;
                }

                if (values[i - this.minValueCount] < lowerThreshold) {
                    exceedsLower--;
                }
            }

            if (exceedsUpper >= this.minExceedCount || exceedsLower >= this.minExceedCount) {
                return true;
            }
        }

        return false;
    }
}

class NelsonOut1SeConstraint extends NelsonOutSeConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.OUT1_SE, typeSelector, 5, 1, 4, instanceIdFilter, limitFeatureSelection);
    }
}

class NelsonOut2SeConstraint extends NelsonOutSeConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.OUT2_SE, typeSelector, 3, 2, 2, instanceIdFilter, limitFeatureSelection);
    }
}

class NelsonOut3SeConstraint extends NelsonOutSeConstraint {
    constructor(feature, typeSelector, instanceIdFilter, limitFeatureSelection) {
        super(feature, NelsonConstraint.NELSON_TYPE.OUT3_SE, typeSelector, 1, 3, 1, instanceIdFilter, limitFeatureSelection);
    }
}

module.exports = {
    NelsonConstraint,
    NelsonOut1SeConstraint,
    NelsonOut2SeConstraint,
    NelsonOut3SeConstraint,
    NelsonBiasConstraint,
    NelsonTrendConstraint,
    NelsonAlterConstraint,
    NelsonLowDevConstraint,
    NelsonHighDevConstraint
};