#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#define SHADER(x) "#version 150 core\n" #x
GLFWwindow *window;
GLuint shader;
int H, W;
float *col;
bool running;
int cur_col;

GLFWwindow *setupWindow(void);
GLuint setupVertexShader(void);
GLuint setupFragementShader(void);

//Assumes sequence of 1D data
void insertCol(const float *data);

//Poll opengl and liblo
void idle(void);

int main();

#define CHECK_OPENGL_ERROR \
{ GLenum error; \
      while ( (error = glGetError()) != GL_NO_ERROR) { \
              printf( "OpenGL ERROR: %s\nCHECK POINT: %s (line %d)\n", \
                            gluErrorString(error), __FILE__, __LINE__ ); \
            } \
}


GLFWwindow *setupWindow(void)
{
    if (glfwInit() != GL_TRUE) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }
    CHECK_OPENGL_ERROR;
    
    // Create a rendering window with OpenGL 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    W = 1024;
    H = 1024;
    GLFWwindow* window_ = glfwCreateWindow(W, H, "OpenGL", NULL, NULL);
    glfwMakeContextCurrent(window_);

    // Init GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        fprintf(stderr, "This May be due to an inability to use OpenGL 3.2+\n");
        return NULL;
    }
    CHECK_OPENGL_ERROR;

    // Create Vertex Array Object
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    CHECK_OPENGL_ERROR;

    // Create a Vertex Buffer Object and copy the vertex data to it
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    CHECK_OPENGL_ERROR;

    static float vertices[] = {
    //  Position      Color             Texcoords
        -1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, // Top-left
         1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Top-right
         1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, // Bottom-right
        -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f  // Bottom-left
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    CHECK_OPENGL_ERROR;

    // Create an Element Buffer Object and copy the element data to it
    GLuint ebo;
    glGenBuffers(1, &ebo);
    CHECK_OPENGL_ERROR;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    CHECK_OPENGL_ERROR;

    static GLuint elements[] = {
         0, 1, 2,
         2, 3, 0
    };

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements,
        GL_STATIC_DRAW);

    GLuint vertexShader = setupVertexShader();
    GLuint fragShader   = setupFragementShader();
    CHECK_OPENGL_ERROR;
    
    shader= glCreateProgram();
    glAttachShader(shader, vertexShader);
    CHECK_OPENGL_ERROR;
    glAttachShader(shader, fragShader);
    CHECK_OPENGL_ERROR;
    glBindFragDataLocation(shader, 0, "outColor");
    CHECK_OPENGL_ERROR;
    glLinkProgram(shader);
    CHECK_OPENGL_ERROR;
    glUseProgram(shader);
    CHECK_OPENGL_ERROR;

    //Initialize Canvas
    GLuint textures[1];
    glGenTextures(1, textures);
    CHECK_OPENGL_ERROR;

    int comp = 0;
    unsigned char* img = malloc(4*W*H);
    for(int i=0; i<W*H; ++i) {
        img[4*i+0] = 0xff;//rand();
        img[4*i+1] = rand();
        img[4*i+2] = rand();
        img[4*i+3] = 0xff;
    }
    glActiveTexture(GL_TEXTURE0);
    CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    CHECK_OPENGL_ERROR;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    CHECK_OPENGL_ERROR;
    glUniform1i(glGetUniformLocation(shader, "tex0"), 0);
    CHECK_OPENGL_ERROR;
    glUniform1f(glGetUniformLocation(shader, "rot"), 0);
    CHECK_OPENGL_ERROR;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    CHECK_OPENGL_ERROR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    CHECK_OPENGL_ERROR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    CHECK_OPENGL_ERROR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CHECK_OPENGL_ERROR;
    free(img);

    // Specify the layout of the vertex data
    GLint posAttrib = glGetAttribLocation(shader, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), 0);

    GLint colAttrib = glGetAttribLocation(shader, "color");
    glEnableVertexAttribArray(colAttrib);
    glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    GLint texAttrib = glGetAttribLocation(shader, "texcoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (void*)(5 * sizeof(GLfloat)));

    return window_;
}

//------------------------------------------------------------------------------
GLuint setupVertexShader(void)
{
    const char* vertexSource = SHADER(

        in vec2 position;
        in vec3 color;
        in vec2 texcoord;

        out vec3 Color;
        out vec2 Texcoord;

        void main() {
            Color = color;
            Texcoord = texcoord;
            gl_Position = vec4(position, 0.0, 1.0);
        }
    );

    GLuint shade = glCreateShader(GL_VERTEX_SHADER);
    CHECK_OPENGL_ERROR;
    glShaderSource(shade, 1, &vertexSource, NULL);
    CHECK_OPENGL_ERROR;
    glCompileShader(shade);
    CHECK_OPENGL_ERROR;
    return shade;
}

//------------------------------------------------------------------------------
GLuint setupFragementShader(void)
{
    const char* fragmentSource = SHADER(

        uniform float rot;
        uniform sampler2D tex0;

        in vec3 Color;
        in vec2 Texcoord;

        out vec4 outColor;

        void main() {
            outColor = texture(tex0, vec2(-Texcoord.x+rot, Texcoord.y)) * vec4(Color, 1.0);
        }
    );

    GLuint shade = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shade, 1, &fragmentSource, NULL);
    glCompileShader(shade);
    CHECK_OPENGL_ERROR;
    return shade;

}

//------------------------------------------------------------------------------
void insertCol(const float *data)
{
    unsigned char *tmp = malloc(4*H);
    for(int i=0; i<H; ++i) {
        int v = 127.0*data[i];
        tmp[4*i+0] = v;//R
        tmp[4*i+1] = v;//B
        tmp[4*i+2] = v;//G
        tmp[4*i+3] = 255;  //A
    }
            
    glTexSubImage2D( GL_TEXTURE_2D, 0, cur_col, 0, 1, H, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
    CHECK_OPENGL_ERROR;
    glUniform1f(glGetUniformLocation(shader, "rot"), cur_col*1.0/W);
    CHECK_OPENGL_ERROR;
    cur_col = (cur_col+1)%W;
    free(tmp);
}
//------------------------------------------------------------------------------

void idle(void)
{
    if(!col)
        col = malloc(4*H);
    //Temporary fake data source
    static int x = 0;
    int inc = (rand()&0xf) - 8;
    if(x+inc > H)
        x = (x+inc)%H;
    else if(x + inc < 0)
        x = x+inc + H;
    else
        x += inc;

    for(int i=0; i<H; ++i)
        col[i] = expf(-abs(x-i)/50.0);
    insertCol(col);

    //normal idle
    running = !glfwWindowShouldClose(window);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK_OPENGL_ERROR;
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK_OPENGL_ERROR;
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    CHECK_OPENGL_ERROR;
    glfwSwapBuffers(window);
    CHECK_OPENGL_ERROR;
    glfwPollEvents();
    CHECK_OPENGL_ERROR;
    printf(".");fflush(stdout);
}

//------------------------------------------------------------------------------



int main()
{
    //Setup Window/Shaders
    window = setupWindow();
    if(!window)
        return 1;
    
    //Run idle until done
    running = true;
    while(running)
        idle();
    
    glfwTerminate();
    return 0;
}
