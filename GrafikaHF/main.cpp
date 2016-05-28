#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>		// must be downloaded
#include <GL/freeglut.h>	// must be downloaded unless you have an Apple
#endif

const unsigned int windowWidth = 600, windowHeight = 600;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Innentol modosithatod...

// OpenGL major and minor versions
int majorVersion = 3, minorVersion = 0;

void getErrorInfo(unsigned int handle) {
    int logLen;
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
    if (logLen > 0) {
        char * log = new char[logLen];
        int written;
        glGetShaderInfoLog(handle, logLen, &written, log);
        printf("Shader log:\n%s", log);
        delete log;
    }
}

// check if shader could be compiled
void checkShader(unsigned int shader, char * message) {
    int OK;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
    if (!OK) {
        printf("%s!\n", message);
        getErrorInfo(shader);
    }
}

// check if shader could be linked
void checkLinking(unsigned int program) {
    int OK;
    glGetProgramiv(program, GL_LINK_STATUS, &OK);
    if (!OK) {
        printf("Failed to link shader program!\n");
        getErrorInfo(program);
    }
}

// vertex shader in GLSL
const char *vertexSource = R"(
#version 140
precision highp float;

uniform mat4 MVP;			// Model-View-Projection matrix in row-major format

in vec2 vertexPosition;		// variable input from Attrib Array selected by glBindAttribLocation
in vec3 vertexColor;	    // variable input from Attrib Array selected by glBindAttribLocation
out vec3 color;				// output attribute

void main() {
    color = vertexColor;														// copy color from input to output
    gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 		// transform to clipping space
}
)";

// fragment shader in GLSL
const char *fragmentSource = R"(
#version 140
precision highp float;

in vec3 color;				// variable input: interpolated color of vertex shader
out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

void main() {
    fragmentColor = vec4(color, 1); // extend RGB to RGBA
}
)";

// row-major matrix 4x4
struct mat4 {
    float m[4][4];
public:
    mat4() {}
    mat4(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33) {
        m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
        m[1][0] = m10; m[1][1] = m11; m[1][2] = m02; m[1][3] = m13;
        m[2][0] = m20; m[2][1] = m21; m[2][2] = m02; m[2][3] = m23;
        m[3][0] = m30; m[3][1] = m31; m[3][2] = m02; m[3][3] = m33;
    }
    
    mat4 operator*(const mat4& right) {
        mat4 result;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                result.m[i][j] = 0;
                for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
            }
        }
        return result;
    }
    operator float*() { return &m[0][0]; }
};


// 3D point in homogeneous coordinates
struct vec4 {
    float v[4];
    
    vec4(float x = 0, float y = 0, float z = 0, float w = 1) {
        v[0] = x; v[1] = y; v[2] = z; v[3] = w;
    }
    
    vec4 operator*(const mat4& mat) {
        vec4 result;
        for (int j = 0; j < 4; j++) {
            result.v[j] = 0;
            for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
        }
        return result;
    }
};

struct Coord{
    float x, y;
public:
    Coord(float x=0, float y=0){
        this->x = x;
        this->y = y;
    }
    Coord operator-(const Coord& c){
        return Coord(this->x - c.x, this->y - c.y);
    }
    Coord operator*(const float f){
        return Coord(this->x * f, this->y * f);
    }
    Coord operator/(const float f){
        return Coord(this->x / f, this->y /f);
    }
    Coord operator+(const float f){
        return Coord(this->x + f, this->y + f);
    }
    Coord operator+(const Coord& c){
        return Coord(this->x + c.x, this->y + c.y);
    }
};

float wGx = -15, wGy = -15;

// 2D camera
struct Camera {
    float wCx, wCy;	// center in world coordinates
    float wWx, wWy;	// width and height in world coordinates
    bool shouldIFollow = false;
public:
    Camera() {
        Animate(0);
    }
    
    mat4 V() { // view matrix: translates the center to the origin
        return mat4(1,    0, 0, 0,
                    0,    1, 0, 0,
                    0,    0, 1, 0,
                    -wCx, -wCy, 0, 1);
    }
    
    mat4 P() { // projection matrix: scales it to be a square of edge length 2
        return mat4(2/wWx,    0, 0, 0,
                    0,    2/wWy, 0, 0,
                    0,        0, 1, 0,
                    0,        0, 0, 1);
    }
    
    mat4 Vinv() { // inverse view matrix
        return mat4(1,     0, 0, 0,
                    0,     1, 0, 0,
                    0,     0, 1, 0,
                    wCx, wCy, 0, 1);
    }
    
    mat4 Pinv() { // inverse projection matrix
        return mat4(wWx/2, 0,    0, 0,
                    0, wWy/2, 0, 0,
                    0,  0,    1, 0,
                    0,  0,    0, 1);
    }
    
    void Animate(float t) {
        wWx = 20;
        wWy = 20;
        if (!shouldIFollow){
            wCx = 0; //10 * cosf(t);
            wCy = 0;
        } else {
            wCx = wGx;
            wCy = wGy;
        }
    }
    
    
    void changeViewMode(bool b){
        shouldIFollow = b;
    }
};

// 2D camera
Camera camera;

// handle of the shader program
unsigned int shaderProgram;

int segmentNumber = 0;

class CatmullRomSpline {
    GLuint vao, vbo;        // vertex array object, vertex buffer object
    float  vertexData[20*5];// interleaved data of coordinates and colors
    float  vertexData2[20*5*700];
    int    nVertices;       // number of vertices
    float deltats[20], ts[20];
    Coord a0s[20], a1s[20], a2s[20], a3s[20], vels[20];
    bool starIsAtLastSegment = false;
public:
    CatmullRomSpline() {
        nVertices = 0;
    }
    void Create() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        
        GLuint vbo;	// vertex/index buffer object
        glGenBuffers(1, &vbo); // Generate 1 vertex buffer object
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // Enable the vertex attribute arrays
        glEnableVertexAttribArray(0);  // attribute array 0
        glEnableVertexAttribArray(1);  // attribute array 1
        // Map attribute array 0 to the vertex data of the interleaved vbo
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0)); // attribute array, components/attribute, component type, normalize?, stride, offset
        // Map attribute array 1 to the color data of the interleaved vbo
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    }

    void calcConstants(Coord x0, Coord x1, Coord v0, Coord v1, float deltat, int i){
        a0s[i] = x0;
        a1s[i] = v0;
        a2s[i] = (x1 - x0)*3.0f/(deltat * deltat) - (v1 + v0 * 2.0f)/deltat;
        a3s[i] = (x0-x1)*2.0f/(deltat * deltat * deltat) + (v1+v0) / (deltat * deltat);
    }

    void calcVi(Coord ri, Coord ribefore, Coord riafter, float deltat0, float deltat1, int i){
        vels[i] = ((riafter - ri) / (deltat1) + (ri - ribefore) / (deltat0)) * 0.9;
    }

    Coord catmullRom(float deltat, int i){
        return a3s[i]*(deltat * deltat * deltat) + a2s[i] * (deltat * deltat) + a1s[i] * deltat + a0s[i];
    }

    Coord makeCoordFromVertexData(int i){
        return Coord(vertexData[5 * i], vertexData[5 * i + 1]);
    }
    
    void AddPoint(float cX, float cY) {
        if (nVertices >= 20) return;
        float t = glutGet(GLUT_ELAPSED_TIME);
        if (nVertices == 0){
            deltats[0] = 0.5*1000.0f;
        }
        else{
            deltats[nVertices] = t - ts[nVertices - 1];
        }
        ts[nVertices] = t;
        
        vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
        // fill interleaved data
        vertexData[5 * nVertices]     = wVertex.v[0];
        vertexData[5 * nVertices + 1] = wVertex.v[1];
        vertexData[5 * nVertices + 2] = 1; // red
        vertexData[5 * nVertices + 3] = 1; // green
        vertexData[5 * nVertices + 4] = 1; // blue
        nVertices++;
        
        if (starIsAtLastSegment){
            segmentNumber++;
        }
        
        if (nVertices >= 2){
            for (int i = 0; i < nVertices; i++){
                Coord actualPoint = makeCoordFromVertexData(i);
                if(i == 0 && nVertices == 2){
                    Coord otherPoint = makeCoordFromVertexData(i+1);
                    calcVi(actualPoint, otherPoint, otherPoint, deltats[i], deltats[i+1], i);
                } else {
                    Coord beforePoint = makeCoordFromVertexData(i-1);
                    if(i == nVertices - 1){
                        Coord afterPoint = makeCoordFromVertexData(0);
                        calcVi(actualPoint, beforePoint, afterPoint, deltats[i], deltats[0], i);
                    } else {
                        Coord afterPoint = makeCoordFromVertexData(i+1);
                        calcVi(actualPoint, beforePoint, afterPoint, deltats[i], deltats[i+1], i);
                    }
                }
            }
            for(int i=0; i<nVertices; i++){
                if (i == nVertices - 1){
                    calcConstants(makeCoordFromVertexData(i), makeCoordFromVertexData(0), vels[i], vels[0], deltats[0], i);
                } else {
                    calcConstants(makeCoordFromVertexData(i), makeCoordFromVertexData(i+1), vels[i], vels[i+1], deltats[i + 1], i);
                }
                for(int j = 0; j<700; j++){
                    Coord CatmullRom;
                    if(i == nVertices - 1){
                        CatmullRom = catmullRom((deltats[0]/700.0f) * j, i);
                    } else {
                        CatmullRom = catmullRom((deltats[i+1]/700.0f) * j, i);
                    }
                    vertexData2[5*700*i + 5*j] = CatmullRom.x;
                    vertexData2[5*700*i + 5*j + 1] = CatmullRom.y;
                    vertexData2[5*700*i + 5*j + 2] = 1;
                    vertexData2[5*700*i + 5*j + 3] = 0;
                    vertexData2[5*700*i + 5*j + 4] = 0;
                }
            }
        }
        // copy data to the GPU
        if (nVertices <= 1){
            glBufferData(GL_ARRAY_BUFFER, nVertices * 5 * sizeof(float), vertexData, GL_DYNAMIC_DRAW);
        } else {
            glBufferData(GL_ARRAY_BUFFER, nVertices * 700 * 5 * sizeof(float), vertexData2, GL_DYNAMIC_DRAW);
        }
    }
    
    void Draw() {
        if (nVertices > 0) {
            mat4 VPTransform = camera.V() * camera.P();
            
            int location = glGetUniformLocation(shaderProgram, "MVP");
            if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, VPTransform);
            else printf("uniform MVP cannot be set\n");
            
            glBindVertexArray(vao);
            if(nVertices <= 1){
                glDrawArrays(GL_LINE_STRIP, 0, nVertices);
            } else {
                glDrawArrays(GL_LINE_STRIP, 0, nVertices * 700);
            }
        }
    }
    
    int getNrOfVertices(){
        return nVertices;
    }
    
    int segmentNumberFromT(float t){
        for (int i= 0; i<nVertices-1; i++){
            if(ts[i+1]>t)
                return i;
        }
        return nVertices-1;
    }
    
    Coord posWhenT(float t, float* t0){
        float deltat = t - *t0;
        if (segmentNumber == nVertices - 1){
            if (deltat >= deltats[0]){
                segmentNumber = 0;
                *t0 = t;
                starIsAtLastSegment = false;
            }
        } else {
            if (deltat >= deltats[segmentNumber+1]){
                segmentNumber++;
                *t0 = t;
                if (segmentNumber == nVertices-1){
                    starIsAtLastSegment = true;
                }
            }
        }
        deltat = t - *t0;
        return catmullRom(deltat, segmentNumber);
    }
};

class Star {
    unsigned int vao;	// vertex array object id
    float sx, sy;		// scaling
    float wTx = -15, wTy = -15;		// translation
    float rsinz, rcosz;
    bool isOnScreen = false;
    CatmullRomSpline* spline;
    float t0 = 0;
    float vertexColors[24*3];
    float s = 0;
public:
    Star() {
        Animate(0);
    }
    
    void Create() {
        glGenVertexArrays(1, &vao);	// create 1 vertex array object

        glBindVertexArray(vao);		// make it active
        
        unsigned int vbo[2];		// vertex buffer objects
        glGenBuffers(2, &vbo[0]);	// Generate 2 vertex buffer objects
        
        // vertex coordinates: vbo[0] -> Attrib Array 0 -> vertexPosition of the vertex shader
        glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); // make it active, it is an array
        static float vertexCoords[] = { 0, 2, -0.5, 0, 0.5, 0,
            1.41, 1.41, 0.35, -0.35, -0.35, 0.35,
            2, 0, 0, 0.5, 0, -0.5,
            1.41, -1.41, 0.35, 0.35, -0.35, -0.35,
            0, -2, -0.5, 0, 0.5, 0,
            -1.41, -1.41, 0.35, -0.35, -0.35, 0.35,
            -2, 0, 0, 0.5, 0, -0.5,
            -1.41, 1.41, 0.35, 0.35, -0.35, -0.35};	// vertex data on the CPU
        glBufferData(GL_ARRAY_BUFFER,      // copy to the GPU
                     sizeof(vertexCoords), // number of the vbo in bytes
                     vertexCoords,		   // address of the data array on the CPU
                     GL_STATIC_DRAW);	   // copy to that part of the memory which is not modified
        // Map Attribute Array 0 to the current bound vertex buffer (vbo[0])
        glEnableVertexAttribArray(0);
        // Data organization of Attribute Array 0
        glVertexAttribPointer(0,			// Attribute Array 0
                              2, GL_FLOAT,  // components/attribute, component type
                              GL_FALSE,		// not in fixed point format, do not normalized
                              0, NULL);     // stride and offset: it is tightly packed
        
        // vertex colors: vbo[1] -> Attrib Array 1 -> vertexColor of the vertex shader
        glBindBuffer(GL_ARRAY_BUFFER, vbo[1]); // make it active, it is an array
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexColors), vertexColors, GL_STATIC_DRAW);	// copy to the GPU
        
        // Map Attribute Array 1 to the current bound vertex buffer (vbo[1])
        glEnableVertexAttribArray(1);  // Vertex position
        // Data organization of Attribute Array 1
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL); // Attribute Array 1, components/attribute, component type, normalize?, tightly packed
    }
    
    void Animate(float t) {
        sx = fabs(sinf(t)); // *sinf(t);
        sy = fabs(sinf(t)); // *cosf(t);
        rsinz = sinf(t*4);
        rcosz = cosf(t*4);
        if(spline!= NULL){
            makeItGoOnCatmull(t);
        } else {
            if(isOnScreen){
                makeItAttrackToShiny();
            }
        }
    }
    
    void Draw() {
        mat4 rotationM(rcosz, -rsinz, 0, 0,
                       rsinz, rcosz, 0, 0,
                       0, 0, 1, 0,
                       wTx, wTy, 0, 1);
        mat4 M(sx,   0,  0, 0,
               0,  sy,  0, 0,
               0,   0,  0, 0,
               0, 0,  0, 1); // model matrix
        
        M = M * rotationM;
        mat4 MVPTransform = M * camera.V() * camera.P();
        
        // set GPU uniform matrix variable MVP with the content of CPU variable MVPTransform
        int location = glGetUniformLocation(shaderProgram, "MVP");
        if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVPTransform); // set uniform variable MVP to the MVPTransform
        else printf("uniform MVP cannot be set\n");
        
        glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
        glDrawArrays(GL_TRIANGLES, 0, 24);
    }
    
    void makeItGoOnCatmull(float t){
        float tms = t * 1000.0f;
        if (spline->getNrOfVertices() >= 2){
            if (t0 == 0){
                t0 = tms;
            }
            Coord temp = spline->posWhenT(tms, &t0);
            wTx = temp.x;
            wTy = temp.y;
            wGx = temp.x;
            wGy = temp.y;
        }
    }
    
    void setCoordinatesFirstTime(float cX, float cY){
        if (isOnScreen){
            return;
        } else {
            vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
            if(spline != NULL){
                wTx = wVertex.v[0];
                wTy = wVertex.v[1];
            } else {
                wTx = (-0.8) * wVertex.v[0];
                wTy = (-0.8) * wVertex.v[1];
            }
            isOnScreen = true;
        }
    }
    
    void setSpline(CatmullRomSpline* crs){
        spline = crs;
    }
    
    void setColor(float r, float g, float b){
        for (int i = 0; i<24*3; i++){
            switch (i%3){
                case 0: vertexColors[i] = r;
                    break;
                case 1: vertexColors[i] = g;
                    break;
                case 2: vertexColors[i] = b;
                    break;
                default: vertexColors[i] = 1;
                    break;
            }
        }
    }
    
    void makeItAttrackToShiny(){
        float d = sqrtf((wGx - wTx) * (wGx - wTx) + (wGy - wTy) * (wGy - wTy));
        if (d < 1){
            d = 1;
        }
        float a = 0.007 / (d * d);
        s += a;
        s *= 0.95;
        if (wTx - wGx < 0){
            wTx += s;
        } else {
            wTx -= s;
        }
        if (wTy - wGy < 0){
            wTy += s;
        } else {
            wTy -= s;
        }
    }
};

// The virtual world: collection of two objects
Star shinyStar;
Star notSoShinyStar;
Star definitelyNotShinyStar;
CatmullRomSpline lineStrip;

// Initialization, create an OpenGL context
void onInitialization() {
    glViewport(0, 0, windowWidth, windowHeight);
    
    // Create objects by setting up their vertex data on the GPU
    shinyStar.setColor(1, 1, 1);
    shinyStar.Create();
    shinyStar.setSpline(&lineStrip);
    notSoShinyStar.setColor(1, 1, 0);
    notSoShinyStar.Create();
    definitelyNotShinyStar.setColor(1, 0, 0.8);
    definitelyNotShinyStar.Create();
    lineStrip.Create();

    
    // Create vertex shader from string
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (!vertexShader) {
        printf("Error in vertex shader creation\n");
        exit(1);
    }
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    checkShader(vertexShader, "Vertex shader error");
    
    // Create fragment shader from string
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fragmentShader) {
        printf("Error in fragment shader creation\n");
        exit(1);
    }
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    checkShader(fragmentShader, "Fragment shader error");
    
    // Attach shaders to a single program
    shaderProgram = glCreateProgram();
    if (!shaderProgram) {
        printf("Error in shader program creation\n");
        exit(1);
    }
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    
    // Connect Attrib Arrays to input variables of the vertex shader
    glBindAttribLocation(shaderProgram, 0, "vertexPosition"); // vertexPosition gets values from Attrib Array 0
    glBindAttribLocation(shaderProgram, 1, "vertexColor");    // vertexColor gets values from Attrib Array 1
    
    // Connect the fragmentColor to the frame buffer memory
    glBindFragDataLocation(shaderProgram, 0, "fragmentColor");	// fragmentColor goes to the frame buffer memory
    
    // program packaging
    glLinkProgram(shaderProgram);
    checkLinking(shaderProgram);
    // make this program run
    glUseProgram(shaderProgram);
}

void onExit() {
    glDeleteProgram(shaderProgram);
    printf("exit");
}

// Window has become invalid: Redraw
void onDisplay() {
    glClearColor(0.7, 0.8, 0.7, 0);							// background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);         // clear the screen
    
    shinyStar.Draw();
    notSoShinyStar.Draw();
    definitelyNotShinyStar.Draw();
    lineStrip.Draw();
    glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
    if (key == ' '){
        camera.changeViewMode(true);
    }
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
    if (key == ' '){
        camera.changeViewMode(false);
    }
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {  // GLUT_LEFT_BUTTON / GLUT_RIGHT_BUTTON and GLUT_DOWN / GLUT_UP
        float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
        float cY = 1.0f - 2.0f * pY / windowHeight;
        lineStrip.AddPoint(cX, cY);
        shinyStar.setCoordinatesFirstTime(cX, cY);
        notSoShinyStar.setCoordinatesFirstTime(cX, cY+0.2);
        definitelyNotShinyStar.setCoordinatesFirstTime(cX-0.2, cY-0.1);
        glutPostRedisplay();     // redraw
    }
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
    long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
    float sec = time / 1000.0f;				// convert msec to sec
    camera.Animate(sec);					// animate the camera
    shinyStar.Animate(sec);					// animate the triangle object
    notSoShinyStar.Animate(sec);
    definitelyNotShinyStar.Animate(sec);
    glutPostRedisplay();					// redraw the scene
}

// Idaig modosithatod...
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int main(int argc, char * argv[]) {
    glutInit(&argc, argv);
#if !defined(__APPLE__)
    glutInitContextVersion(majorVersion, minorVersion);
#endif
    glutInitWindowSize(windowWidth, windowHeight);				// Application window is initially of resolution 600x600
    glutInitWindowPosition(100, 100);							// Relative location of the application window
#if defined(__APPLE__)
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);  // 8 bit R,G,B,A + double buffer + depth buffer
#else
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
    glutCreateWindow(argv[0]);
#if !defined(__APPLE__)
    glewExperimental = true;	// magic
    glewInit();
#endif
    printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
    printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
    printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
    printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
    printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    onInitialization();
    
    glutDisplayFunc(onDisplay);                // Register event handlers
    glutMouseFunc(onMouse);
    glutIdleFunc(onIdle);
    glutKeyboardFunc(onKeyboard);
    glutKeyboardUpFunc(onKeyboardUp);
    glutMotionFunc(onMouseMotion);
    
    glutMainLoop();
    onExit();
    return 1;
}
