const fs = require('fs');
const path = require('path');
const glob = require('glob');
const UglifyJS = require('uglify-js');
const yargs = require('yargs');

const defaultOptions = {
    compress: {},
    mangle: {},
    output: {
        comments: false
    }
}

function minifyAllJsFiles(inDir, inGlob, outDir, options = defaultOptions) {
    inDir = path.resolve(inDir);
    inGlob = `${inDir}/${inGlob}`;
    const inFiles = glob.sync(inGlob).map(file => path.resolve(file));

    for (let inFile of inFiles) {
        console.log(`Input: ${inFile}`);

        const result = UglifyJS.minify(
            fs.readFileSync(inFile, { encoding: 'utf8', flag: 'r'}),
            options
        );

        const outFile = inFile.replace(path.resolve(inDir), path.resolve(outDir));

        try {
            fs.mkdirSync(path.dirname(outFile), { recursive: true });
        }
        catch(err) {
            //
        }

        fs.writeFileSync(outFile, result.code, { encoding: 'utf8'});

        console.log(`Output: ${outFile}`);
    }
}

const args = yargs
    .option('inDir', {
        alias: 'i',
        type: 'string',
        description: 'Input directory',
        required: true
    })
    .option('inGlob', {
        alias: 'g',
        type: 'string',
        description: 'Input glob pattern',
        required: true
    })
    .option('outDir', {
        alias: 'o',
        type: 'string',
        description: 'Output directory',
        required: true
    })
    .help()
    .alias('help', 'h');

const argv = args.argv;

minifyAllJsFiles(argv.inDir, argv.inGlob, argv.outDir);