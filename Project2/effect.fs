#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;    
    float shininess;
	//index of refraction 
	float IOR;
}; 

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
in vec3 FragPos;  
in vec3 Normal;  
in vec2 TexCoords;
in vec3 Position;
uniform vec3 viewPos;

uniform Light light;
uniform samplerCube skybox;
uniform Material material;
//calculate f0 component for fresnel approximation 
//see reference https://google.github.io/filament/Filament.html
float F0 = ((material.IOR-1)*(material.IOR-1))/ ((material.IOR+1)*(material.IOR+1));

const float ratioR = 0.71;
const float ratioG =0.69;
const float ratioB =0.68;



void main()
{
    vec3 I = normalize(Position - viewPos);
	//reflection
    vec3 Reflect = reflect(I, normalize(Normal));
	vec3 ReflectRes = vec3(texture(skybox, Reflect));

	//refraction
	//vec3 Refract = refract(I, normalize(Normal), ratio);
	// chromatic aberration effect
	vec3 RRefractionVec = refract(I, normalize(Normal), ratioR);
    vec3 GRefractionVec = refract(I, normalize(Normal), ratioG);
    vec3 BRefractionVec = refract(I, normalize(Normal), ratioB);
	
	vec3 RefractRes;
    RefractRes.r = vec3(texture(skybox, RRefractionVec)).r;
    RefractRes.g = vec3(texture(skybox, GRefractionVec)).g;
    RefractRes.b = vec3(texture(skybox, BRefractionVec)).b;

	
	//fresnel ratio
	float fresnel = F0 + (1.0 - F0) * pow((1.0 - dot(-I, normalize(Normal))), 5.0);
	
	//combine together
	vec3 result = mix(RefractRes, ReflectRes, fresnel);
    FragColor = vec4(result, 1.0);
}