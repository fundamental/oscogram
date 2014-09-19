#include <lo/lo.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <fftw3.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#define SHADER(x) "#version 150 core\n" #x
lo_server server;
GLFWwindow *window;
GLuint shader;
int H, W;
float *col;
bool running;
fftw_plan plan;
int cur_col;

GLFWwindow *setupWindow(void);
GLuint setupVertexShader(void);
GLuint setupFragementShader(void);

//Assumes sequence of 1D data
void insertCol(const float *data);

void lo_setup(void);
int handler_function(const char *, const char *,
                     lo_arg **, int, lo_message, void *);

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


//------------------------------------------------------------------------------
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

    W = 1024*4;
    H = 1024+512;
    GLFWwindow* window_ = glfwCreateWindow(W, H, "OpenGL", NULL, NULL);
    glfwMakeContextCurrent(window_);

    // Init GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        fprintf(stderr, "This May be due to an inability to use OpenGL 3.2+\n");
        return NULL;
    }
    glGetError();
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
    unsigned char* img = (unsigned char*)malloc(4*W*H);
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
    unsigned char *tmp = (unsigned char*)malloc(4*H);
    for(int i=0; i<H; ++i) {
        float d = data[i];//(logf(data[i]/10)+1);
        if(d<0)
            d = 0;
        if(d > 1) {
            d = 1;
        }

        int v = 127.0*d;
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
fftw_plan plans[32];
double fft_in[32][200];
fftw_complex fft_out[32][400];
double fft_mag[32][200];
void lo_setup(void)
{
    for(int i=0; i<32; ++i)
        plans[i] = fftw_plan_dft_r2c_1d(200, fft_in[i], fft_out[i], FFTW_ESTIMATE);
    server = lo_server_new_with_proto("4321", LO_UDP, NULL);
    lo_server_add_method(server, NULL, NULL, handler_function, NULL);
    printf("oscogram running on port %d\n", lo_server_get_port(server));
}

//------------------------------------------------------------------------------
int   fft_pos[32];
static void build_fft(int i, float f)
{
    fft_in[i][fft_pos[i]++] = f;
    fft_pos[i] %= 200;
}
static bool ready_fft(int i)
{
    return fft_pos[i] % 50 == 0;
}
static void do_fft(int i)
{
    fftw_execute(plans[i]);
    for(int j=0; j<200; ++j)
        fft_mag[i][j] = sqrt(powf(fft_out[i][j][0],2.0) + powf(fft_out[i][j][1],2.0));
    //fft_pos[i] = 0;
}

int handler_function(const char *path, const char *args,
                     lo_arg **a, int x, lo_message y, void *z)
{
    static float mean_[32] = {0};
    static float std_[32]  = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    printf("*");
    if(!strcmp(path, "/raw_data") && !strcmp(args, "b"))
    {
        lo_blob b = a[0];
    } else if(!strcmp(path, "/multiplot")) {
        const unsigned plots = strlen(args);
        const unsigned pH = H/plots;
        int do_update = 1;
        memset(col, 0, H*4);
        for(unsigned i=0; i<plots; ++i) {
            //Estimate current min/max and mean
            mean_[i] = mean_[i]*0.99 + a[i]->f*0.01;
            std_[i]  = std_[i]*0.99 + fabs(a[i]->f-mean_[i])*0.01;
            float in = (a[i]->f-mean_[i])/(4*std_[i]);
            if(in < -1)
                in = -1;
            if(in > 1)
                in = 1;
            //build_fft(i,in);
            //if(ready_fft(i)) {
            //    do_fft(i);
            //    for(unsigned j=0; j<pH; ++j)
            //        if(j<100)
            //            col[j+pH*i] = fft_mag[i][j];//loc == j ? 1.0 : 0.0;
            //    do_update = 1;
            //}

            const unsigned loc = pH*(1-in)/2;
            for(unsigned j=0; j<pH; ++j)
                col[j+pH*i] = loc == j ? 1.0 : 0.0;
        }
        if(do_update)
            insertCol(col);
        //printf("men:");
        //for(unsigned i=0; i<plots; ++i)
        //    printf("%f, ", mean_[i]);
        //printf("\n");

        //printf("std:");
        //for(unsigned i=0; i<plots; ++i)
        //    printf("%f, ", std_[i]);
        //printf("\n");
    }
    return 0;
}
//------------------------------------------------------------------------------
void idle(void)
{
    if(!col)
        col = (float*)malloc(4*H);
    ////Temporary fake data source
    //static int x = 0;
    //int inc = (rand()&0xf) - 8;
    //if(x+inc > H)
    //    x = (x+inc)%H;
    //else if(x + inc < 0)
    //    x = x+inc + H;
    //else
    //    x += inc;

    //for(int i=0; i<H; ++i)
    //    col[i] = expf(-abs(x-i)/50.0);
    //insertCol(col);

    int x = 100;
    while(lo_server_recv_noblock(server, 1)&& x--);

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
    if(!window) {
        printf("Failed to obtain window\n");
        return 1;
    }
    lo_setup();

    //Run idle until done
    running = true;
    while(running)
        idle();

    printf("Exiting...\n");
    glfwTerminate();
    return 0;
}
