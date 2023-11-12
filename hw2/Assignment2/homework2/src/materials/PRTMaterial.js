class PRTMaterial extends Material {

    constructor(vertexShader, fragmentShader) {
        
        super({
            'uPrecomputeL[0]': { type: 'a', value: null },
            'uPrecomputeL[1]': { type: 'a', value: null },
            'uPrecomputeL[2]': { type: 'a', value: null },

        }, ['aPrecomputeLT'], vertexShader, fragmentShader);
    }
}

async function buildPRTMaterial(vertexPath, fragmentPath) {


    let vertexShader = await getShaderString(vertexPath);
    let fragmentShader = await getShaderString(fragmentPath);

    return new PRTMaterial(vertexShader, fragmentShader);

}