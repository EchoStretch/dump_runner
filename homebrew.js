async function main() {
    const CWD = window.workingDir;
    const PAYLOAD = CWD + '/dump_runner.elf';
    const PARAM_JSON_URL = baseURL + '/fs/' + CWD + '/sce_sys/param.json';

    // Defaults (param.sfo behavior)
    let mainText = 'Dump Runner';
    let secondaryText = CWD.split('/').pop();
    let args = [];

    try {
        // Try param.json first
        const resp = await fetch(PARAM_JSON_URL);

        if (resp.ok) {
            const param = await resp.json();

            // Find English title
            let name = '';
            if (param.localizedParameters) {
                for (const key in param.localizedParameters) {
                    if (key.startsWith('en-')) {
                        name = param.localizedParameters[key].titleName;
                        break;
                    }
                }
            }

            if (name) mainText = name;
            if (param.titleId) secondaryText = param.titleId;

            args = [PAYLOAD, param.titleId];
        }
    } catch (e) {
        // param.json not present → silently fall back to param.sfo behavior
    }

    return {
        mainText,
        secondaryText,
        onclick: async () => {
            return {
                path: PAYLOAD,
                cwd: CWD,
                args,
                daemon: true,
            };
        }
    };
}
