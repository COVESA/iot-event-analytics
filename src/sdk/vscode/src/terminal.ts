const os = require('os');
const { spawn } = require('child_process');

export class Terminal {
    platform: string;

    constructor(platform = os.platform()) {
        // 'aix', 'darwin', 'freebsd', 'linux', 'openbsd', 'sunos', and 'win32'
        this.platform = platform;
    }

    executeCommand(cmd: string, args: string[], cwd: string, onStdOut = (msg: string) => {}) {
        return new Promise((resolve, reject) => {
            const proc = spawn(cmd, args, {
                cwd,
                shell: true
            });

            let stdout = '';
            let stderr = '';

            proc.stdout.on('data', (buf: Buffer) => {
                const stringifiedBuffer = buf.toString('utf8');
                onStdOut(stringifiedBuffer);
                stdout += stringifiedBuffer;
            });

            proc.stderr.on('data', (buf: Buffer) => {
                stderr += buf.toString('utf8');
            });

            proc.on('close', (code: number) => {
                if (code !== 0) {
                    reject(stderr.trim());
                    return;
                }

                resolve(stdout.trim());
            });
        });
    }
}
