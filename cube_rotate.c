#include <GL/glut.h>
#include <math.h>

float rotX = 0.0f;
float rotY = 0.0f;
int lastX, lastY;
int isDragging = 0;

// ViewCube 相關變數
#define VIEWCUBE_SIZE 80
#define VIEWCUBE_MARGIN 20
int viewcube_hover_type = 0; // 0:無, 1:面, 2:邊, 3:角
int viewcube_hover_id = -1;  // 哪一個面/邊/角

// 方向向量表
float face_dirs[6][3] = {
    { 1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0,-1, 0}, {0, 0, 1}, {0, 0,-1}
};
float face_angles[6][2] = {
    { 0, 90}, { 0,-90}, {-90, 0}, {90, 0}, {0, 0}, {0, 180}
};

void init() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void drawCube() {
    glutSolidCube(1.0);
}

// 判斷滑鼠是否在 ViewCube 上，並回傳區域型態與 id
void checkViewCubeHover(int x, int y, int winWidth, int winHeight) {
    int vx = winWidth - VIEWCUBE_SIZE - VIEWCUBE_MARGIN;
    int vy = winHeight - VIEWCUBE_SIZE - VIEWCUBE_MARGIN;
    if (x >= vx && x <= vx + VIEWCUBE_SIZE && y >= vy && y <= vy + VIEWCUBE_SIZE) {
        int cx = x - vx;
        int cy = y - vy;
        float fx = (float)cx / VIEWCUBE_SIZE;
        float fy = (float)cy / VIEWCUBE_SIZE;
        float dx = fx - 0.5f;
        float dy = fy - 0.5f;
        float dist = sqrt(dx*dx + dy*dy);

        // 角落判斷
        if (dist > 0.47 && dist <= 0.5) {
            viewcube_hover_type = 3; // 角
            // 8個角，根據象限判斷
            int idx = (dx > 0 ? 1 : 0) + (dy > 0 ? 2 : 0) + (fx > 0.5f ? 4 : 0);
            viewcube_hover_id = idx % 8;
        }
        // 邊判斷
        else if (dist > 0.4 && dist <= 0.47) {
            viewcube_hover_type = 2; // 邊
            // 12條邊，根據角度判斷
            float angle = atan2(dy, dx) * 180.0f / M_PI;
            int idx = ((int)((angle + 180 + 15) / 30)) % 12;
            viewcube_hover_id = idx;
        }
        // 面判斷
        else if (dist <= 0.4) {
            viewcube_hover_type = 1; // 面
            // 6個面，根據方向判斷
            float max_dot = -2.0f;
            int max_id = 0;
            float v[3] = {dx, dy, 0.5f};
            float len = sqrt(dx*dx + dy*dy + 0.25f);
            v[0] /= len; v[1] /= len; v[2] /= len;
            for (int i = 0; i < 6; ++i) {
                float dot = v[0]*face_dirs[i][0] + v[1]*face_dirs[i][1] + v[2]*face_dirs[i][2];
                if (dot > max_dot) { max_dot = dot; max_id = i; }
            }
            viewcube_hover_id = max_id;
        }
        else {
            viewcube_hover_type = 0;
            viewcube_hover_id = -1;
        }
    } else {
        viewcube_hover_type = 0;
        viewcube_hover_id = -1;
    }
}

// 繪製右上角的 ViewCube
void drawViewCube(int winWidth, int winHeight) {
    glViewport(winWidth - VIEWCUBE_SIZE - VIEWCUBE_MARGIN, winHeight - VIEWCUBE_SIZE - VIEWCUBE_MARGIN, VIEWCUBE_SIZE, VIEWCUBE_SIZE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(30.0, 1.0, 1.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(2,2,2, 0,0,0, 0,1,0);

    // hover 效果
    if (viewcube_hover_type == 1) glColor3f(1,0.8,0.2); // 面
    else if (viewcube_hover_type == 2) glColor3f(0.2,1,0.2); // 邊
    else if (viewcube_hover_type == 3) glColor3f(1,0.2,0.2); // 角
    else glColor3f(0.8,0.8,0.8);

    glutSolidCube(1.0);

    // 還原 viewport
    glViewport(0, 0, winWidth, winHeight);
}

// 點擊時切換主視角與 ViewCube
void mouseButton(int button, int state, int x, int y) {
    int winWidth = glutGet(GLUT_WINDOW_WIDTH);
    int winHeight = glutGet(GLUT_WINDOW_HEIGHT);
    checkViewCubeHover(x, y, winWidth, winHeight);

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && viewcube_hover_type != 0) {
        // 點擊 ViewCube
        if (viewcube_hover_type == 1 && viewcube_hover_id >= 0 && viewcube_hover_id < 6) {
            // 面：直接對應
            rotX = face_angles[viewcube_hover_id][0];
            rotY = face_angles[viewcube_hover_id][1];
        } else if (viewcube_hover_type == 2 && viewcube_hover_id >= 0 && viewcube_hover_id < 12) {
            // 邊：平均兩面
            int a = viewcube_hover_id / 2;
            int b = (viewcube_hover_id + 1) % 6;
            rotX = (face_angles[a][0] + face_angles[b][0]) / 2.0f;
            rotY = (face_angles[a][1] + face_angles[b][1]) / 2.0f;
        } else if (viewcube_hover_type == 3 && viewcube_hover_id >= 0 && viewcube_hover_id < 8) {
            // 角：三面平均
            int a = viewcube_hover_id % 6;
            int b = (viewcube_hover_id + 1) % 6;
            int c = (viewcube_hover_id + 2) % 6;
            rotX = (face_angles[a][0] + face_angles[b][0] + face_angles[c][0]) / 3.0f;
            rotY = (face_angles[a][1] + face_angles[b][1] + face_angles[c][1]) / 3.0f;
        }
        glutPostRedisplay();
        return;
    }
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isDragging = 1;
            lastX = x;
            lastY = y;
        } else {
            isDragging = 0;
        }
    }
}

void mouseMotion(int x, int y) {
    int winWidth = glutGet(GLUT_WINDOW_WIDTH);
    int winHeight = glutGet(GLUT_WINDOW_HEIGHT);
    checkViewCubeHover(x, y, winWidth, winHeight);

    if (viewcube_hover_type != 0) {
        // hover 在 ViewCube 上，不旋轉主立方體
        glutPostRedisplay();
        return;
    }
    if (isDragging) {
        rotY += (x - lastX);
        rotX += (y - lastY);
        lastX = x;
        lastY = y;
        glutPostRedisplay();
    }
}

void mousePassiveMotion(int x, int y) {
    int winWidth = glutGet(GLUT_WINDOW_WIDTH);
    int winHeight = glutGet(GLUT_WINDOW_HEIGHT);
    int prev_type = viewcube_hover_type;
    int prev_id = viewcube_hover_id;
    checkViewCubeHover(x, y, winWidth, winHeight);
    // 只有在 hover 狀態改變時才重繪
    if (prev_type != viewcube_hover_type || prev_id != viewcube_hover_id) {
        glutPostRedisplay();
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(rotX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);

    glColor3f(0.2f, 0.8f, 1.0f);
    drawCube();

    glutSwapBuffers();

    // 取得視窗大小
    int winWidth = glutGet(GLUT_WINDOW_WIDTH);
    int winHeight = glutGet(GLUT_WINDOW_HEIGHT);
    drawViewCube(winWidth, winHeight);
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    float aspect = (float)w / (float)h;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, aspect, 1.0f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Rotating Cube with Mouse");

    init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(mousePassiveMotion); // 新增這行

    glutMainLoop();
    return 0;
}
