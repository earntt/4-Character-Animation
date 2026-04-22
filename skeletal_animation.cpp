#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>

#include <iostream>
#include <map>
#include <string>

// Animation state enum
enum class AnimationState {
	IDLE,
	WALK,
	JUMP,
	DANCE
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void updateAnimationState();
void updateCharacterPhysics();
void printControlsInfo();

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// character animation and position
glm::vec3 characterPosition = glm::vec3(0.0f, -0.4f, 0.0f);
glm::vec3 characterDirection = glm::vec3(0.0f, 0.0f, -1.0f);
float characterSpeed = 1.5f;
bool isMoving = false;
bool isJumping = false;
float jumpVelocity = 0.0f;
const float JUMP_FORCE = 3.5f;
const float GRAVITY = 9.8f;

// animation management
std::map<std::string, Animation*> animations;
AnimationState currentState = AnimationState::IDLE;
AnimationState previousState = AnimationState::IDLE;
Animator* animator = nullptr;

int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
	stbi_set_flip_vertically_on_load(true);

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// build and compile shaders
	// -------------------------
	Shader ourShader("anim_model.vs", "anim_model.fs");

	// load models
	// -----------
	Model ourModel(FileSystem::getPath("resources/objects/pirate/character.dae"));

	Animation idleAnimation(FileSystem::getPath("resources/objects/pirate/Idle.dae"), &ourModel);
	Animation walkAnimation(FileSystem::getPath("resources/objects/pirate/Walking.dae"), &ourModel);
	Animation jumpAnimation(FileSystem::getPath("resources/objects/pirate/Jump.dae"), &ourModel);
	Animation danceAnimation(FileSystem::getPath("resources/objects/pirate/HouseDancing.dae"), &ourModel);

	// Store animations in a map
	animations["idle"] = &idleAnimation;
	animations["walk"] = &walkAnimation;
	animations["jump"] = &jumpAnimation;
	animations["dance"] = &danceAnimation;

	animator = new Animator(&idleAnimation);
	currentState = AnimationState::IDLE;

	printControlsInfo();

	// draw in wireframe
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		// update physics and animation state
		updateCharacterPhysics();
		updateAnimationState();

		animator->UpdateAnimation(deltaTime);

		// render
		// ------
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// don't forget to enable shader before setting uniforms
		ourShader.use();

		// view/projection transformations
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		ourShader.setMat4("projection", projection);
		ourShader.setMat4("view", view);

		auto transforms = animator->GetFinalBoneMatrices();
		for (int i = 0; i < transforms.size(); ++i)
			ourShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

		// render the loaded model
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, characterPosition);
		model = glm::scale(model, glm::vec3(.5f, .5f, .5f));
		ourShader.setMat4("model", model);
		ourModel.Draw(ourShader);

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	delete animator;
	glfwTerminate();
	return 0;
}

// Update character physics (gravity, jumping)
// -------------------------------------------
void updateCharacterPhysics()
{
	if (isJumping)
	{
		// Apply gravity
		jumpVelocity -= GRAVITY * deltaTime;
		characterPosition.y += jumpVelocity * deltaTime;

		// Check if character landed
		if (characterPosition.y <= -0.4f)
		{
			characterPosition.y = -0.4f;
			isJumping = false;
			jumpVelocity = 0.0f;
		}
	}
}

// Update animation state based on character movement
// --------------------------------------------------
void updateAnimationState()
{
	AnimationState newState = currentState;

	if (isJumping)
	{
		newState = AnimationState::JUMP;
	}
	else if (isMoving)
	{
		newState = AnimationState::WALK;
	}
	else if (currentState != AnimationState::DANCE)
	{
		newState = AnimationState::IDLE;
	}

	// If state changed, switch animation
	if (newState != previousState)
	{
		previousState = currentState;
		currentState = newState;

		switch (newState)
		{
		case AnimationState::IDLE:
			if (animations.find("idle") != animations.end())
				animator->PlayAnimation(animations["idle"]);
			break;
		case AnimationState::WALK:
			if (animations.find("walk") != animations.end())
				animator->PlayAnimation(animations["walk"]);
			break;
		case AnimationState::JUMP:
			if (animations.find("jump") != animations.end())
				animator->PlayAnimation(animations["jump"]);
			break;
		case AnimationState::DANCE:
			if (animations.find("dance") != animations.end())
				animator->PlayAnimation(animations["dance"]);
			break;
		}
	}
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	// Camera movement
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);

	// Character movement with Arrow Keys
	isMoving = false;

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && !isJumping)
	{
		characterPosition += characterDirection * characterSpeed * deltaTime;
		isMoving = true;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && !isJumping)
	{
		characterPosition -= characterDirection * characterSpeed * deltaTime;
		isMoving = true;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS && !isJumping)
	{
		glm::vec3 right = glm::normalize(glm::cross(characterDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
		characterPosition -= right * characterSpeed * deltaTime;
		isMoving = true;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS && !isJumping)
	{
		glm::vec3 right = glm::normalize(glm::cross(characterDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
		characterPosition += right * characterSpeed * deltaTime;
		isMoving = true;
	}

	// Jump with Spacebar
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !isJumping)
	{
		isJumping = true;
		jumpVelocity = JUMP_FORCE;
	}

	// Dance animation with number key 1
	if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
	{
		if (currentState != AnimationState::DANCE && animations.find("dance") != animations.end())
		{
			previousState = currentState;
			currentState = AnimationState::DANCE;
			animator->PlayAnimation(animations["dance"]);
		}
	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

// Print controls information to console
// -------------------------------------
void printControlsInfo()
{
	std::cout << "\n======================================" << std::endl;
	std::cout << "  Playable Character Animation System" << std::endl;
	std::cout << "======================================" << std::endl;
	std::cout << "\nCAMERA CONTROLS:" << std::endl;
	std::cout << "  W/A/S/D     - Move camera" << std::endl;
	std::cout << "  Mouse       - Look around" << std::endl;
	std::cout << "  Scroll      - Zoom in/out" << std::endl;
	std::cout << "  ESC         - Exit" << std::endl;

	std::cout << "\nCHARACTER CONTROLS:" << std::endl;
	std::cout << "  UP Arrow    - Move forward" << std::endl;
	std::cout << "  DOWN Arrow  - Move backward" << std::endl;
	std::cout << "  LEFT Arrow  - Move left" << std::endl;
	std::cout << "  RIGHT Arrow - Move right" << std::endl;
	std::cout << "  SPACE       - Jump" << std::endl;
	std::cout << "  1           - Dance" << std::endl;
	std::cout << "  (Standing still = Idle animation)" << std::endl;

	std::cout << "======================================\n" << std::endl;
}
