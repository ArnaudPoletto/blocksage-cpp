const char *baseVertexShaderSource = R"(
    #version 460 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    
    out vec3 vertexColor;
    
    uniform mat4 mvpMatrix;
    
    void main() {
        gl_Position = mvpMatrix * vec4(aPos, 1.0);
        vertexColor = aColor;
    }
    )";

const char *axesFragmentShaderSource = R"(
    #version 460 core
    in vec3 vertexColor;
    out vec4 FragColor;
    
    void main() {
        FragColor = vec4(vertexColor, 1.0);
    }
    )";

const char *cubeVertexShaderSource = R"(
    #version 460 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    layout (location = 2) in vec3 instancePos;
    layout (location = 3) in vec3 aNormal;  // Add normal attribute
    
    out vec3 vertexColor;
    out vec3 fragNormal;   // Pass normal to fragment shader
    out vec3 fragPos;      // Pass fragment position for lighting calculations
    
    uniform mat4 viewProjectionMatrix;
    
    void main() {
        mat4 model = mat4(1.0);
        model[3] = vec4(instancePos, 1.0);
        
        // Calculate world position
        vec4 worldPos = model * vec4(aPos, 1.0);
        gl_Position = viewProjectionMatrix * worldPos;
        
        // Pass values to fragment shader
        vertexColor = aColor;
        fragNormal = mat3(model) * aNormal;  // Transform normal to world space
        fragPos = worldPos.xyz;
    }
    )";

const char *cubeFragmentShaderSource = R"(
    #version 460 core
    in vec3 vertexColor;
    in vec3 fragNormal;   // Receive normal from vertex shader
    in vec3 fragPos;      // Receive fragment position
    
    out vec4 FragColor;
    
    uniform vec3 colorOverride;
    uniform bool useColorOverride;
    uniform vec3 lightDir;    // Direction of the light
    uniform vec3 viewPos;     // Camera position for optional specular highlights
    
    void main() {
        // Normalize the incoming normal
        vec3 normal = normalize(fragNormal);
        
        // Base color (either from vertex color or override)
        vec3 baseColor = useColorOverride ? colorOverride : vertexColor;
        
        // Calculate lighting
        // 1. Ambient component (constant light)
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * baseColor;
        
        // 2. Diffuse component (directional light)
        vec3 lightDirection = normalize(lightDir);
        float diff = max(dot(normal, lightDirection), 0.0);
        vec3 diffuse = diff * baseColor;
        
        // Combine lighting components
        vec3 finalColor = ambient + diffuse;
        
        // Output the final color
        FragColor = vec4(finalColor, 1.0);
    }
    )";