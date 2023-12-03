#include <Windows.h>
#include "shader.hpp"
#include "controls.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imGui/imgui.h"
#include "imGui/imgui_impl_glfw.h"
#include "imGui/imgui_impl_opengl3.h"

#include "camera.h"
#include "model.h"

#include "utils.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <unordered_map>

#include <cstdlib>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>

int windowWidth = 1280;
int windowHeight = 800;
GLFWwindow* window;
glm::vec2 viewXY = glm::vec2(372,794);
GLuint loadShaders(const char * vertex_file_path, const char * fragment_file_path);
// Lens flare parameters

int   downsample = 2;
float thresholdBias = -0.9f;
float thresholdScale = 9.0f;
float ghostsAlpha = 1.0f;
int   ghostsNumber = 3;
float ghostsDispersal = 0.7f;
float ghostsWeight = 27.5f;
float haloAlpha = 1.0f;
float haloWidth = 0.75f;
float haloWeight = 0.125f;
float chromaticDistortion = -7.5f;
float blurRadius = 3.0f;
float starAlpha = 9.0f;
float dustAlpha = 0.35f;
float colorGradientAlpha = 0.5f;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float camera_zoomin = 75.0f;
float lastX = windowWidth / 2.0f;
float lastY = windowHeight / 2.0f;
bool firstMouse = true;
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;
	viewXY.x = lastX;
	viewXY.y = -lastY;
	camera.ProcessMouseMovement(xoffset, yoffset);
}

GLuint loadCubemap(std::string faces[])
{
	GLenum format;
	GLuint textureID;
	int channels;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height;
	for (unsigned int i = 0; i < 6; i++)
	{
		unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &channels, 0);
		if (!data) {
			fprintf(stderr, "%s %s\n", "Failed to Load Texture", faces[i].c_str());
			return 0;
		}
		if (data)
		{
			
			switch (channels) {
			case 1: format = GL_ALPHA;     break;
			case 2: format = GL_LUMINANCE; break;
			case 3: format = GL_RGB;       break;
			case 4: format = GL_RGBA;      break;
			}
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else
		{
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	
	return textureID;
};
GLuint loadImage(std::string filename, int &textureWidth, int &textureHeight) {
	GLenum format;
	GLuint texture;
	int channels;
	unsigned char * image = stbi_load(filename.c_str(), &textureWidth, &textureHeight, &channels, 0);
	if (!image) {
		fprintf(stderr, "%s %s\n", "Failed to Load Texture", filename.c_str());
		return 0;
	}
	switch (channels) {
	case 1: format = GL_ALPHA;     break;
	case 2: format = GL_LUMINANCE; break;
	case 3: format = GL_RGB;       break;
	case 4: format = GL_RGBA;      break;
	}
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, format, textureWidth, textureHeight, 0, format, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(image);
	cout << filename.c_str() << "  loaded" << endl;
	return texture;
}

int main(int argc, char * argv[]) {

	glfwInit();
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	window = glfwCreateWindow(windowWidth, windowHeight, "Lens FLare", nullptr, nullptr);
	if (window == nullptr) {
		fprintf(stderr, "Failed to Create OpenGL Context");
		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);
	gladLoadGL();
	fprintf(stderr, "OpenGL %s\n", glGetString(GL_VERSION));
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwPollEvents();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	setupControls(window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	const char* glsl_version = "#version 330";
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	ImGui::StyleColorsDark();

	

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	int w, h; // unused
	GLuint gradientTexture = loadImage("D:/Visual Studio Project/rendering final/Project2/resources/lenscolor.png", w, h);
	GLuint dustTexture = loadImage("D:/Visual Studio Project/rendering final/Project2/resources/lensdirt.png", w, h);
	GLuint starTexture = loadImage("D:/Visual Studio Project/rendering final/Project2/resources/lensstar.png", w, h);
	

	

//#pragma mark Set up example image

	int textureWidth;
	int textureHeight;

	GLuint texture = loadImage("D:/Visual Studio Project/Lens Flare/Project2/resources/posz.jpg", textureWidth, textureHeight);
	//GLuint cubetexture = loadCubemap(faces);
	//GLuint cubemapTexture = loadCubemap(faces);
	//cout << cubetexture << endl;
	GLuint readFramebufferID = 0;
	glGenFramebuffers(1, &readFramebufferID);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferID);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

//#pragma mark Fullscreen quad buffer

	static const GLfloat fullscreenQuad[] = {
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
	};

	GLuint fullscreenQuadId;
	glGenBuffers(1, &fullscreenQuadId);
	glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadId);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenQuad), fullscreenQuad, GL_STATIC_DRAW);



//#pragma mark Framebuffers
//
//#pragma mark .... renderFrameBuffer

	// The scene is rendered to this framebuffer.
	// Its texture is then used to generate lens flare artifacts,
	// and is combined with those artifacts in the final pass.

	GLuint renderFrameBuffer = 0;
	glGenFramebuffers(1, &renderFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, renderFrameBuffer);

	GLuint renderTexture;
	glGenTextures(1, &renderTexture);
	glBindTexture(GL_TEXTURE_2D, renderTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderTexture, 0);

	GLenum renderDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, renderDrawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { return 0; }



//#pragma mark .... lensFlareFrameBuffer

	// This lens flare artifacts are rendered to this framebuffer,
	// then it is reused as a target during blur,
	// then finally combined with the scene in the final pass.

	GLuint lensFlareFrameBuffer = 0;
	glGenFramebuffers(1, &lensFlareFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, lensFlareFrameBuffer);

	GLuint lensFlareTexture;
	glGenTextures(1, &lensFlareTexture);
	glBindTexture(GL_TEXTURE_2D, lensFlareTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth / downsample, windowHeight / downsample, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, lensFlareTexture, 0);

	GLenum lensFlareDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, lensFlareDrawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { return 0; }



//#pragma mark .... blurFrameBuffer

	// This framebuffer is used during blur passes.

	GLuint blurFrameBuffer = 0;
	glGenFramebuffers(1, &blurFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, blurFrameBuffer);

	GLuint blurTexture;
	glGenTextures(1, &blurTexture);
	glBindTexture(GL_TEXTURE_2D, blurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth / downsample, windowHeight / downsample, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, blurTexture, 0);

	GLenum blurDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, blurDrawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { return 0; }



//#pragma mark Lens flare program

	GLuint lensflareID = loadShaders("../Project2/pass.vs", "../Project2/lensflare.fs");
	GLuint aspectRatioID = glGetUniformLocation(lensflareID, "aspectRatio");
	GLuint chromaticDistortionID = glGetUniformLocation(lensflareID, "chromaticDistortion");
	GLuint colorGradientAlphaID = glGetUniformLocation(lensflareID, "colorGradientAlpha");
	GLuint dustAlphaID = glGetUniformLocation(lensflareID, "dustAlpha");
	GLuint dustTextureID = glGetUniformLocation(lensflareID, "dustTexture");
	GLuint ghostsAlphaID = glGetUniformLocation(lensflareID, "ghostsAlpha");
	GLuint ghostsDispersalID = glGetUniformLocation(lensflareID, "ghostsDispersal");
	GLuint ghostsWeightID = glGetUniformLocation(lensflareID, "ghostsWeight");
	GLuint ghostsNumberID = glGetUniformLocation(lensflareID, "ghostsNumber");
	GLuint gradientTextureID = glGetUniformLocation(lensflareID, "gradientTexture");
	GLuint haloAlphaID = glGetUniformLocation(lensflareID, "haloAlpha");
	GLuint haloWidthID = glGetUniformLocation(lensflareID, "haloWidth");
	GLuint haloWeightID = glGetUniformLocation(lensflareID, "haloWeight");
	GLuint renderTextureID = glGetUniformLocation(lensflareID, "renderTexture");
	GLuint starAlphaID = glGetUniformLocation(lensflareID, "starAlpha");
	GLuint starRotationID = glGetUniformLocation(lensflareID, "starRotation");
	GLuint starTextureID = glGetUniformLocation(lensflareID, "starTexture");
	GLuint thesholdBiasID = glGetUniformLocation(lensflareID, "thesholdBias");
	GLuint thresholdScaleID = glGetUniformLocation(lensflareID, "thesholdScale");

	



//#pragma mark Blur program

	GLuint blurID = loadShaders("../Project2/pass.vs", "../Project2/blur.fs");
	GLuint blurTextureID = glGetUniformLocation(blurID, "blurTexture");
	GLuint blurDirectionID = glGetUniformLocation(blurID, "direction");
	GLuint blurRadiusID = glGetUniformLocation(blurID, "radius");
	GLuint blurResolutionID = glGetUniformLocation(blurID, "resolution");



//#pragma mark Blend program

	GLuint blendID = loadShaders("../Project2/pass.vs", "../Project2/blend.fs");
	GLuint sceneTextureID = glGetUniformLocation(blendID, "sceneTexture");
	GLuint effectsTextureID = glGetUniformLocation(blendID, "effectsTexture");



#pragma mark Render loop
	//Render loop
	while (glfwWindowShouldClose(window) == false) {

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);
	

		updateControls(window, windowWidth, windowHeight, textureWidth, textureHeight, viewXY);
		/*glfwSetCursorPosCallback(window, mouse_callback);
		glfwSetScrollCallback(window, scroll_callback);*/


		// tell GLFW to capture our mouse
		//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		// Our state
	
		{
			ImGui::Begin("Lens Flare");
			ImGui::SliderFloat("Threshold Bias", &thresholdBias, -1.0f, 1.0f);
			ImGui::SliderFloat("Threshold Scale", &thresholdScale, 0.0f, 20.0f);
			ImGui::SliderFloat("Ghosts Alpha", &ghostsAlpha, 0.0f, 10.0f);
			ImGui::SliderInt("Ghosts Number", &ghostsNumber, 0, 10);
			ImGui::SliderFloat("Ghosts Dispersal", &ghostsDispersal, 0.0f, 2.0f);
			ImGui::SliderFloat("Ghosts Weight", &ghostsWeight, 0.0f, 50.0f);
			ImGui::SliderFloat("Halo Alpha", &haloAlpha, 0.0f, 10.0f);
			ImGui::SliderFloat("Halo Width", &haloWidth, -1.0f, 1.0f);
			ImGui::SliderFloat("Halo Weight", &haloWeight, 0.0f, 1.0f);
			ImGui::SliderFloat("Chromatic Distortion", &chromaticDistortion, -100.0f, 100.0f);
			ImGui::SliderFloat("Star Alpha", &starAlpha, 0.0f, 10.0f);
			ImGui::SliderFloat("Color Gradient Alpha", &colorGradientAlpha, 0.0f, 1.0f);
			ImGui::SliderFloat("Dust Alpha", &dustAlpha, 0.0f, 1.0f);
			ImGui::SliderFloat("Blur Radius", &blurRadius, 0.0f, 10.0f);
			ImGui::End();
		}
//#pragma mark .... Render example image

		glBindFramebuffer(GL_FRAMEBUFFER, renderFrameBuffer);
		glViewport(0, 0, windowWidth, windowHeight);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebufferID);
		glBlitFramebuffer(
			viewXY.x, viewXY.y,
			2.0f * windowWidth + viewXY.x, 2.0f * windowHeight + viewXY.y,
			0, 0, 2.0f * windowWidth, 2.0f * windowHeight,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		cout << viewXY.x<<"        "<<viewXY.y << endl;

//#pragma mark .... Render lens flare artifacts (read renderTexture, write lensFlareFrameBuffer)

		glBindFramebuffer(GL_FRAMEBUFFER, lensFlareFrameBuffer);
		glViewport(0, 0, windowWidth / downsample, windowHeight / downsample);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(lensflareID);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, renderTexture);
		glUniform1i(renderTextureID, 0);

		glUniform1f(thesholdBiasID, thresholdBias);
		glUniform1f(thresholdScaleID, thresholdScale);
		glUniform1f(ghostsAlphaID, ghostsAlpha);
		glUniform1i(ghostsNumberID, ghostsNumber);
		glUniform1f(ghostsDispersalID, ghostsDispersal);
		glUniform1f(ghostsWeightID, ghostsWeight);
		glUniform1f(haloAlphaID, haloAlpha);
		glUniform1f(haloWidthID, haloWidth);
		glUniform1f(haloWeightID, haloWeight);
		glUniform1f(chromaticDistortionID, chromaticDistortion);
		glUniform1f(aspectRatioID, (float)windowWidth / (float)windowHeight);
		glUniform1f(starAlphaID, starAlpha);
		glUniform1f(dustAlphaID, dustAlpha);
		glUniform1f(colorGradientAlphaID, colorGradientAlpha);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gradientTexture);
		glUniform1i(gradientTextureID, 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, dustTexture);
		glUniform1i(dustTextureID, 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, starTexture);
		glUniform1i(starTextureID, 3);

		// Rotate lens flare's starburst as camera moves
		// When rendering a 3D scene, should use the view matrix to calculate starburst's rotation:
		//glm::vec4 camx4 = glm::column(ViewMatrix,0); // camera x (left) vector
		//glm::vec4 camz4 = glm::column(ViewMatrix,1); // camera z (forward) vector
		//glm::vec3 camx = vec3(camx4.x,camx4.y,camx4.z);
		//glm::vec3 camz = vec3(camz4.x,camz4.y,camz4.z);
		//float camrot = glm::dot(camx, vec3(0,0,1)) + glm::dot(camz, vec3(0,1,0));
		float camrot = 0.01f * ((float)viewXY.x - (float)viewXY.y);
		glm::mat4 scaleBias1 = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5, 0.0f));
		glm::mat4 rotation = glm::mat4(1.0f);
		rotation = glm::rotate(rotation, camrot, glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 scaleBias2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5, 0.0f));
		glm::mat4 uLensStarMatrix = scaleBias2 * rotation * scaleBias1;
		glUniformMatrix4fv(starRotationID, 1, GL_FALSE, &uLensStarMatrix[0][0]);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadId);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(0);



//#pragma mark .... Render horizontal blur pass (read lensFlareTexture, write blurFrameBuffer)

		glBindFramebuffer(GL_FRAMEBUFFER, blurFrameBuffer);
		glViewport(0, 0, windowWidth / downsample, windowHeight / downsample);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(blurID);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, lensFlareTexture);
		glUniform1i(blurTextureID, 0);
		glUniform2f(blurDirectionID, 1.0f, 0.0f);
		glUniform1f(blurRadiusID, blurRadius);
		glUniform1f(blurResolutionID, (float)windowWidth);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadId);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(0);


//#pragma mark .... Render vertical blur pass (read blurTexture, write lensFlareFrameBuffer)

		glBindFramebuffer(GL_FRAMEBUFFER, lensFlareFrameBuffer);
		glViewport(0, 0, windowWidth / downsample, windowHeight / downsample);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(blurID);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, blurTexture);
		glUniform1i(blurTextureID, 0);
		glUniform2f(blurDirectionID, 0.0f, 1.0f);
		glUniform1f(blurRadiusID, blurRadius);
		glUniform1f(blurResolutionID, (float)windowHeight);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadId);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(0);



//#pragma mark .... Render to screen (read renderTexture & lensFlareTexture, write screen)

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, windowWidth, windowHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(blendID);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, renderTexture);
		glUniform1i(sceneTextureID, 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, lensFlareTexture);
		glUniform1i(effectsTextureID, 1);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, fullscreenQuadId);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		
		glDisableVertexAttribArray(0);

		////projection view and model matrix
		//glm::mat4 projectionMatrix = glm::perspective(camera_zoomin, (float)windowWidth / windowHeight, 0.01f, 100.0f);
		//glm::mat4 viewMatrix = camera.GetViewMatrix();
		//glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
		////skybox
		//// draw skybox as last
		//glDepthFunc(GL_LEQUAL);
		//skyboxShader.use();
		//glm::mat4 view_sky = glm::mat4(glm::mat3(camera.GetViewMatrix())); // remove translation from the view matrix
		//skyboxShader.setMat4("view", view_sky);
		//skyboxShader.setMat4("projection", projectionMatrix);
		//// skybox cube
		//glBindVertexArray(skyboxVAO);
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
		//glDrawArrays(GL_TRIANGLES, 0, 36);
		//
		//glBindVertexArray(0);
		//glDepthFunc(GL_LESS);


		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();

	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwTerminate();
	return EXIT_SUCCESS;
}
