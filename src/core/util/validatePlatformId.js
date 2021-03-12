module.exports = function validatePlatformId(platformId) {
    if (typeof(platformId) !== 'string' || platformId.trim().length === 0) {
        throw new Error(`PlatformId must be of type string and at least 1 character long`);
    }
};