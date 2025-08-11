// cube_chars.c
// Compile: gcc cube_chars.c -o cube_chars -lGL -lGLU -lglut -lm
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/glut.h>
#include <stdio.h>

GLuint texIDs[6];
const char* filenames[6] = {
    "up.png",    // +Z in our scene
    "down.png",  // -Z
    "left.png",  // -X
    "right.png", // +X
    "front.png", // +Y
    "back.png"   // -Y
};

float rotX = 20.0f; // main scene rotation
float rotY = -25.0f;
int lastX = 0, lastY = 0;
int dragging = 0;
int cubeDragging = 0;
float zoom = 6.0f; // initial camera distance

int hoverInCube = 0;
int hoverScreenX = 0, hoverScreenY = 0; // in pixels, relative to hovered viewport

typedef enum {
    FACE_NONE = -1,
    FACE_POS_X,
    FACE_NEG_X,
    FACE_POS_Y,
    FACE_NEG_Y,
    FACE_POS_Z,
    FACE_NEG_Z
} FaceID;
FaceID hoveredFace = FACE_NONE;

// torus parameters (tweak to taste)
float torusInnerRadius = 0.12f; // tube radius
float torusOuterRadius = 1.6f;  // distance from origin to tube center
int   torusSides = 32;
int   torusRings = 48;

GLuint texDonut[4]; // 0:East, 1:West, 2:South, 3:North

GLuint loadTextureFromFile(const char* filename) {
    int w, h, channels;
    unsigned char* data = stbi_load(filename, &w, &h, &channels, 4);
    if (!data) {
        fprintf(stderr, "Failed to load %s\n", filename);
        return 0;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    return texID;
}

FaceID pickCubeFace(int mx, int my, int winW, int winH,
                    int cubeX, int cubeY, int cubeSize,
                    float rotX_deg, float rotY_deg)
{
    // Convert mouse to normalized device coords in cube viewport
    if (mx < cubeX || mx > cubeX + cubeSize ||
        my < cubeY || my > cubeY + cubeSize)
        return FACE_NONE;

    double ndcX = ((mx - cubeX) / (double)cubeSize) * 2.0 - 1.0;
    double ndcY = ((my - cubeY) / (double)cubeSize) * 2.0 - 1.0;
    ndcY = -ndcY; // OpenGL Y is opposite to window Y

    // Build cube rotation matrix
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -6.0f);
    glRotatef(rotX_deg, 1, 0, 0);
    glRotatef(rotY_deg, 0, 1, 0);

    double model[16], proj[16];
    int viewport[4] = {0, 0, cubeSize, cubeSize};
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);

    glPopMatrix();

    // Ray in object space
    GLdouble ox, oy, oz;
    GLdouble tx, ty, tz;
    gluUnProject((mx - cubeX), (my - cubeY), 0.0, model, proj, viewport, &ox, &oy, &oz);
    gluUnProject((mx - cubeX), (my - cubeY), 1.0, model, proj, viewport, &tx, &ty, &tz);

    double dirX = tx - ox;
    double dirY = ty - oy;
    double dirZ = tz - oz;

    // Normalize dir
    double len = sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
    dirX /= len; dirY /= len; dirZ /= len;

    // Cube is centered at origin with half-size s
    double s = 0.8;
    FaceID hitFace = FACE_NONE;
    double nearestT = 1e9;

    // Test each face
    struct { FaceID id; double nx, ny, nz, px, py, pz; } faces[6] = {
        { FACE_POS_X,  1, 0, 0,  s, 0, 0 },
        { FACE_NEG_X, -1, 0, 0, -s, 0, 0 },
        { FACE_POS_Y,  0, 1, 0, 0,  s, 0 },
        { FACE_NEG_Y,  0,-1, 0, 0, -s, 0 },
        { FACE_POS_Z,  0, 0, 1, 0, 0,  s },
        { FACE_NEG_Z,  0, 0,-1, 0, 0, -s }
    };

    for (int i = 0; i < 6; i++) {
        double denom = faces[i].nx*dirX + faces[i].ny*dirY + faces[i].nz*dirZ;
        if (fabs(denom) < 1e-6) continue; // parallel

        double t = ((faces[i].px - ox) * faces[i].nx +
                    (faces[i].py - oy) * faces[i].ny +
                    (faces[i].pz - oz) * faces[i].nz) / denom;

        if (t > 0 && t < nearestT) {
            // Intersection point
            double ix = ox + t*dirX;
            double iy = oy + t*dirY;
            double iz = oz + t*dirZ;

            // Check within bounds of face
            if (fabs(ix - faces[i].px) <= 1e-3 || fabs(iy - faces[i].py) <= s + 1e-3 || fabs(iz - faces[i].pz) <= s + 1e-3) {
                if (fabs(ix) <= s + 1e-3 &&
                    fabs(iy) <= s + 1e-3 &&
                    fabs(iz) <= s + 1e-3) {
                    nearestT = t;
                    hitFace = faces[i].id;
                }
            }
        }
    }

    return hitFace;
}

void snapToFace(FaceID f) {
    switch (f) {
        case FACE_POS_X: rotX = 0;   rotY = -90; break;
        case FACE_NEG_X: rotX = 0;   rotY = 90;  break;
        case FACE_POS_Y: rotX = -90; rotY = 0;   break;
        case FACE_NEG_Y: rotX = 90;  rotY = 0;   break;
        case FACE_POS_Z: rotX = 0;   rotY = 0;   break;
        case FACE_NEG_Z: rotX = 0;   rotY = 180; break;
        default: break;
    }
}

// ViewCube viewport size/offset
int cubeSize = 200;
int cubeOffset = 20;

void loadTextures() {
    glGenTextures(6, texIDs);

    for (int i = 0; i < 6; ++i) {
        int w, h, comp;
        unsigned char *data = stbi_load(filenames[i], &w, &h, &comp, 4);
        if (!data) {
            fprintf(stderr, "Failed to load %s\n", filenames[i]);
            continue;
        }
        glBindTexture(GL_TEXTURE_2D, texIDs[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }
}

void drawTexturedFace(GLuint tex, float size) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f); glVertex3f(-size, -size, 0.0f);
      glTexCoord2f(1.0f, 0.0f); glVertex3f( size, -size, 0.0f);
      glTexCoord2f(1.0f, 1.0f); glVertex3f( size,  size, 0.0f);
      glTexCoord2f(0.0f, 1.0f); glVertex3f(-size,  size, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void drawDonutLabels(float innerR, float outerR, float height) {

    texDonut[0] = loadTextureFromFile("east.png");  // 東 (+X)
    texDonut[1] = loadTextureFromFile("west.png");  // 西 (-X)
    texDonut[2] = loadTextureFromFile("south.png"); // 南 (+Z)
    texDonut[3] = loadTextureFromFile("north.png"); // 北 (-Z)

    float size =(outerR - innerR) / 2.0f; // label size
    float pos = (outerR + innerR) / 2.0f;
    // East (+X) West (-X) South (+Z) North (-Z)
    glPushMatrix(); glTranslatef(pos,0.01f,0); glRotatef(90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[0],size); glPopMatrix();
    glPushMatrix(); glTranslatef(-pos,0.01f,0); glRotatef(90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[1],size); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.01f,pos); glRotatef(90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[2],size); glPopMatrix();
    glPushMatrix(); glTranslatef(0,0.01f,-pos); glRotatef(90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[3],size); glPopMatrix();
    glPushMatrix(); glTranslatef(pos,-0.01f,0); glRotatef(-90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[0],size); glPopMatrix();
    glPushMatrix(); glTranslatef(-pos,-0.01f,0); glRotatef(-90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[1],size); glPopMatrix();
    glPushMatrix(); glTranslatef(0,-0.01f,pos); glRotatef(-90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[2],size); glPopMatrix();
    glPushMatrix(); glTranslatef(0,-0.01f,-pos); glRotatef(-90,1,0,0); glRotatef(0,0,0,1); drawTexturedFace(texDonut[3],size); glPopMatrix();
}

void drawAxes(float length) {
    glLineWidth(2.0f);
    glBegin(GL_LINES);
      glColor3f(1.0f, 0.0f, 0.0f); // X
      glVertex3f(0,0,0); glVertex3f(length,0,0);
      glColor3f(0.0f, 1.0f, 0.0f); // Y
      glVertex3f(0,0,0); glVertex3f(0,length,0);
      glColor3f(0.0f, 0.0f, 1.0f); // Z
      glVertex3f(0,0,0); glVertex3f(0,0,length);
    glEnd();
}

void highlightFaceOverlay(float size) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 0.0f, 0.4f); // yellow tint
    glBegin(GL_QUADS);
      glVertex3f(-size, -size, 0.0f);
      glVertex3f( size, -size, 0.0f);
      glVertex3f( size,  size, 0.0f);
      glVertex3f(-size,  size, 0.0f);
    glEnd();
    glDisable(GL_BLEND);
}

// Draw a flat 2D donut on the XZ plane (Y=0), centered at origin
void drawFlatDonutXZ(float innerR, float outerR, int segments) {
    float angleStep = 2.0f * M_PI / segments;

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cosf(angle);
        float z = sinf(angle);

        // Outer edge
        glVertex3f(x * outerR, 0.0f, z * outerR);
        // Inner edge
        glVertex3f(x * innerR, 0.0f, z * innerR);
    }
    glEnd();
}

void drawViewCube(int winW, int winH) {
    glViewport(winW - cubeSize - cubeOffset, winH - cubeSize - cubeOffset, cubeSize, cubeSize);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(30.0, 1.0, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -8.0f);
    glRotatef(rotX, 1, 0, 0);
    glRotatef(rotY, 0, 1, 0);

    // draw donut on XZ plane around cube
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.8f, 0.8f, 0.8f); // gold-ish
    drawFlatDonutXZ(1.2f, 1.6f, 64);
    drawDonutLabels(1.2f, 1.6f, 0.01f);
    glEnable(GL_TEXTURE_2D);

    glColor3f(0.8f, 0.8f, 0.8f); // Gray

    float s = 0.7f;
    // +X
    glPushMatrix(); glTranslatef(s,0,0); glRotatef(-90,0,1,0); glRotatef(180,0,0,1);drawTexturedFace(texIDs[3],s); 
    if (hoveredFace == FACE_POS_X) highlightFaceOverlay(s);
    glPopMatrix();
    // -X
    glPushMatrix(); glTranslatef(-s,0,0); glRotatef(90,0,1,0); glRotatef(180,0,0,1); drawTexturedFace(texIDs[2],s); 
    if (hoveredFace == FACE_NEG_X) highlightFaceOverlay(s);
    glPopMatrix();
    // +Z
    glPushMatrix(); glTranslatef(0,s,0); glRotatef(90,1,0,0); drawTexturedFace(texIDs[0],s); 
    if (hoveredFace == FACE_POS_Z) highlightFaceOverlay(s);
    glPopMatrix();
    // -Z
    glPushMatrix(); glTranslatef(0,-s,0); glRotatef(-90,1,0,0); drawTexturedFace(texIDs[1],s); 
    if (hoveredFace == FACE_NEG_Z) highlightFaceOverlay(s);
    glPopMatrix();
    // +Y
    glPushMatrix(); glTranslatef(0,0,s);  glRotatef(180,0,1,0); glRotatef(180,0,0,1); drawTexturedFace(texIDs[4],s); 
    if (hoveredFace == FACE_POS_Y) highlightFaceOverlay(s);
    glPopMatrix();
    // -Y
    glPushMatrix(); glTranslatef(0,0,-s); glRotatef(0,0,1,0); glRotatef(180,0,0,1); drawTexturedFace(texIDs[5],s); 
    if (hoveredFace == FACE_NEG_Y) highlightFaceOverlay(s);
    glPopMatrix();
}

void drawStatusText() {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: (%d, %d) px",
             hoverInCube ? "ViewCube" : "Main",
             hoverScreenX, hoverScreenY);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0, 0, 0);
    glRasterPos2i(10, 10);
    for (const char *c = buf; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display(void) {
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Main scene
    glViewport(0,0,winW,winH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)winW/winH, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0,0,-zoom);
    glRotatef(rotX, 1, 0, 0);
    glRotatef(rotY, 0, 1, 0);
    drawAxes(1.0f);

    // ----- Status Overlay -----
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    // Draw ViewCube
    drawViewCube(winW, winH);

    // Restore viewport to full window before drawing text
    glViewport(0, 0, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
    drawStatusText();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h==0) h=1;
    glViewport(0,0,w,h);
}

void mouseButton(int button, int state, int x, int y) {
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    int oglY = winH - y;

    // Mouse wheel zoom (Linux GLUT buttons 3 & 4)
    if (button == 3 && state == GLUT_DOWN) { zoom -= 0.3f; if (zoom < 2) zoom = 2; glutPostRedisplay(); return; }
    if (button == 4 && state == GLUT_DOWN) { zoom += 0.3f; if (zoom > 50) zoom = 50; glutPostRedisplay(); return; }

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int cubeSize = 200;
        int cubeOffset = 50;
        int cubeX = winW - cubeSize - cubeOffset;
        int cubeY = winH - cubeSize - cubeOffset;

        if (x >= cubeX && x <= cubeX + cubeSize &&
            oglY >= cubeY && oglY <= cubeY + cubeSize) {
            // Click inside cube → pick face
            FaceID f = pickCubeFace(x, oglY, winW, winH, cubeX, cubeY, cubeSize, rotX, rotY);

            if (f != FACE_NONE) {
                snapToFace(f);
                glutPostRedisplay();
                return;
            }
        } else {
            dragging = 1;
            lastX = x; lastY = y;
        }
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        dragging = 0;
    }
}

void mouseMotion(int x, int y) {
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    int cubeSize = 200;
    int cubeX = winW - cubeSize - 50;
    int cubeY = winH - cubeSize - 50;

    // Detect hover in ViewCube
    hoverInCube = (x >= cubeX && x <= cubeX + cubeSize &&
                   y >= cubeY && y <= cubeY + cubeSize);

    if (hoverInCube) {
        // Coordinates relative to ViewCube viewport
        hoverScreenX = x - cubeX;
        hoverScreenY = (winH - y) - cubeY; // bottom-left = (0,0)
    } else {
        // Coordinates relative to Main viewport
        hoverScreenX = x;
        hoverScreenY = winH - y; // OpenGL origin bottom-left
    }

    // Rotation dragging
    if (dragging) {
        rotY += (x - lastX) * 0.5f;
        rotX += (y - lastY) * 0.5f;
        lastX = x; lastY = y;
    }

    glutPostRedisplay();
}

void initGL() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    loadTextures();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1200, 800);
    glutCreateWindow("AutoCAD-style ViewCube with Chinese Characters");
    initGL();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(mouseMotion);
    glutMainLoop();
    return 0;
}
