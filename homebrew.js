async function main() {
    const CWD = window.workingDir
    const PAYLOAD = CWD + '/dump_runner.elf';
    const PARAM_URL = baseURL + '/fs/' + CWD + '/sce_sys/param.json';
    
    const resp = await fetch(PARAM_URL);
    const param = await resp.json();

    var name = '';
    for(const key in param.localizedParameters) {
	if(key.startsWith('en-')) {
	    name = param.localizedParameters[key]['titleName'];
	    break;
	}
    }

    return {
	mainText: name,
	secondaryText: param['titleId'],
        onclick: async () => {
	    return {
		path: PAYLOAD,
		cwd: CWD,
		args: [PAYLOAD, param['titleId']],
		daemon: true,
	    };
        }
    };
}
