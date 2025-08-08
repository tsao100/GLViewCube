#include <GL/glut.h>
#include <GL/glu.h>
#include <math.h>

float view_rot_x = 20, view_rot_y = -30, view_rot_z = 0;
float cube_rot_x = 20, cube_rot_y = -30, cube_rot_z = 0;
int dragging = 0, dragging_main = 0, last_x, last_y;
int drag_mode = 0; // 0: xy, 1: z

// Hover 狀態
int hover_type = 0; // 0:無, 1:面, 2:邊, 3:角
int hover_id = -1;

// 方向向量表與對應旋轉
float face_dirs[6][3] = {
    { 1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0,-1, 0}, {0, 0, 1}, {0, 0,-1}
};
float face_angles[6][2] = {
    { 0, 90}, { 0,-90}, {-90, 0}, {90, 0}, {0, 0}, {0, 180}
};

// 每個面的四個頂點（順序：左下、右下、右上、左上）
const float face_vertices[6][4][3] = {
    // +X
    { {0.5,-0.5,-0.5}, {0.5,0.5,-0.5}, {0.5,0.5,0.5}, {0.5,-0.5,0.5} },
    // -X
    { {-0.5,-0.5,0.5}, {-0.5,0.5,0.5}, {-0.5,0.5,-0.5}, {-0.5,-0.5,-0.5} },
    // +Y
    { {-0.5,0.5,-0.5}, {0.5,0.5,-0.5}, {0.5,0.5,0.5}, {-0.5,0.5,0.5} },
    // -Y
    { {-0.5,-0.5,0.5}, {0.5,-0.5,0.5}, {0.5,-0.5,-0.5}, {-0.5,-0.5,-0.5} },
    // +Z
    { {-0.5,-0.5,0.5}, {0.5,-0.5,0.5}, {0.5,0.5,0.5}, {-0.5,0.5,0.5} },
    // -Z
    { {0.5,-0.5,-0.5}, {-0.5,-0.5,-0.5}, {-0.5,0.5,-0.5}, {0.5,0.5,-0.5} }
};

// 每個面的顏色
const float face_colors[6][3] = {
    {1,0,0},    // +X 紅
    {0,1,0},    // -X 綠
    {0,0,1},    // +Y 藍
    {1,1,0},    // -Y 黃
    {1,0,1},    // +Z 紫
    {0,1,1}     // -Z 青
};

// --- 新增：細分面區域 ---
#define FACE_SUBDIV_X 3
#define FACE_SUBDIV_Y 3
#define FACE_CELLS (FACE_SUBDIV_X * FACE_SUBDIV_Y)

// 新增 hover 狀態
int hover_face = -1; // 0~5
int hover_cell = -1; // 0~8 (3x3)

// 雙線性插值
void lerp_face_vertex(const float v[4][3], float u, float vval, float out[3]) {
    float a[3], b[3];
    for (int i = 0; i < 3; ++i) {
        a[i] = v[0][i] * (1-u) + v[1][i] * u;
        b[i] = v[3][i] * (1-u) + v[2][i] * u;
        out[i] = a[i] * (1-vval) + b[i] * vval;
    }
}

// 繪製單一 cell，邊界比例 1:8:1
void drawFaceCell(int face, int cell, int highlight) {
    // 1:8:1 分割點
    float edge_ratio[4] = {0.0f, 0.1f, 0.9f, 1.0f};
    int cx = cell % FACE_SUBDIV_X;
    int cy = cell / FACE_SUBDIV_X;
    float u0 = edge_ratio[cx];
    float u1 = edge_ratio[cx+1];
    float v0 = edge_ratio[cy];
    float v1 = edge_ratio[cy+1];
    float vtx[4][3];
    lerp_face_vertex(face_vertices[face], u0, v0, vtx[0]);
    lerp_face_vertex(face_vertices[face], u1, v0, vtx[1]);
    lerp_face_vertex(face_vertices[face], u1, v1, vtx[2]);
    lerp_face_vertex(face_vertices[face], u0, v1, vtx[3]);
    if (highlight)
        glColor3f(1, 0.8, 0.2);
    else
        glColor3fv(face_colors[face]);
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; ++i)
        glVertex3fv(vtx[i]);
    glEnd();
}

void drawCube(float size) {
    glutSolidCube(size);
}

// 輔助：將螢幕座標轉換為 ViewCube 內部的 3D 空間座標
void screenToViewCubeLocal(int x, int y, int winWidth, int winHeight, int size, float *out) {
    int vx = winWidth - size - 10;
    int vy = winHeight - size - 10; // <--- 修正這裡
    float fx = (float)(x - vx) / size;
    float fy = 1.0f - (float)(y - vy) / size; // OpenGL Y 軸向上

    // 轉換到 [-0.5, 0.5] 區間的 ViewCube 正投影平面
    float px = (fx - 0.5f);
    float py = (fy - 0.5f);

    // 反向套用旋轉矩陣（將螢幕平面點轉回 cube local）
    float rx = -cube_rot_x * M_PI / 180.0f;
    float ry = -cube_rot_y * M_PI / 180.0f;
    float rz = 0; // 若有 z 軸旋轉可加

    // 先繞 Y 軸
    float x1 = cos(ry)*px + sin(ry)*0.5f;
    float z1 = -sin(ry)*px + cos(ry)*0.5f;
    float y1 = py;

    // 再繞 X 軸
    float y2 = cos(rx)*y1 - sin(rx)*z1;
    float z2 = sin(rx)*y1 + cos(rx)*z1;
    float x2 = x1;

    // 若有 z 軸旋轉可再加一段

    out[0] = x2;
    out[1] = y2;
    out[2] = z2;
}

// 更新後的 hover 判斷
void checkViewCubeHover(int x, int y, int winWidth, int winHeight, int size) {
    hover_face = -1;
    hover_cell = -1;
    hover_type = 0;
    hover_id = -1;
    int vx = winWidth - size - 10;
    int vy = winHeight - size - 10;
    if (x < vx || x > vx + size || y < vy || y > vy + size) return;

    float local[3];
    screenToViewCubeLocal(x, y, winWidth, winHeight, size, local);
    float lx = local[0], ly = local[1], lz = local[2];

    // 判斷最近的面
    float max_dot = -2.0f;
    int max_face = -1;
    float norm[6][3] = {
        { 1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0,-1, 0}, {0, 0, 1}, {0, 0,-1}
    };
    float center[6][3] = {
        {0.5,0,0}, {-0.5,0,0}, {0,0.5,0}, {0,-0.5,0}, {0,0,0.5}, {0,0,-0.5}
    };
    float v[3];
    for (int i = 0; i < 6; ++i) {
        v[0] = lx - center[i][0];
        v[1] = ly - center[i][1];
        v[2] = lz - center[i][2];
        float dot = -(v[0]*norm[i][0] + v[1]*norm[i][1] + v[2]*norm[i][2]);
        if (dot > max_dot) { max_dot = dot; max_face = i; }
    }

    // 判斷是否在面上
    float u, vcell;
    int cell_x = -1, cell_y = -1;
    if (max_face == 0 || max_face == 1) { // ±X
        u = (lz + 0.5f);
        vcell = (ly + 0.5f);
    } else if (max_face == 2 || max_face == 3) { // ±Y
        u = (lx + 0.5f);
        vcell = (lz + 0.5f);
    } else { // ±Z
        u = (lx + 0.5f);
        vcell = (ly + 0.5f);
    }
    float edge_ratio[4] = {0.0f, 0.1f, 0.9f, 1.0f};
    // 判斷 cell_x
    for (int i = 0; i < FACE_SUBDIV_X; ++i) {
        if (u >= edge_ratio[i] && u < edge_ratio[i+1]) {
            cell_x = i;
            break;
        }
    }
    // 判斷 cell_y (這裡要用 FACE_SUBDIV_Y)
    for (int i = 0; i < FACE_SUBDIV_Y; ++i) {
        if (vcell >= edge_ratio[i] && vcell < edge_ratio[i+1]) {
            cell_y = i;
            break;
        }
    }
    if (cell_x >= 0 && cell_y >= 0) {
        hover_face = max_face;
        hover_cell = cell_y * FACE_SUBDIV_X + cell_x;
        hover_type = 1;
        hover_id = hover_face * FACE_CELLS + hover_cell;
        return;
    }
    hover_type = 0;
    hover_id = -1;
}

void drawViewCube(int winWidth, int winHeight, int size) {
    glViewport(winWidth - size - 10, winHeight - size - 10, size, size);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(30, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0,0,5, 0,0,0, 0,1,0);

    glPushMatrix();
    glRotatef(cube_rot_z, 0, 0, 1); // 新增
    glRotatef(cube_rot_x, 1, 0, 0);
    glRotatef(cube_rot_y, 0, 1, 0);

    // 每個面細分 3x3
    for (int f = 0; f < 6; ++f) {
        for (int cell = 0; cell < FACE_CELLS; ++cell) {
            int highlight = (hover_type == 1 && hover_face == f && hover_cell == cell);
            drawFaceCell(f, cell, highlight);
        }
    }

    // --- 中文標籤 ---
    const char* face_labels_zh[6] = {"右", "左", "上", "下", "前", "後"};
    float label_pos_zh[6][3] = {
        {0.5f, 0.0f, 0.0f},   // +X 右
        {-0.5f, 0.0f, 0.0f},  // -X 左
        {0.0f, 0.5f, 0.0f},   // +Y 上
        {0.0f, -0.5f, 0.0f},  // -Y 下
        {0.0f, 0.0f, 0.5f},   // +Z 前
        {0.0f, 0.0f, -0.5f}   // -Z 後
    };
    float label_normal[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    float label_up[6][3] = {
        {0,1,0}, {0,1,0}, {0,0,-1}, {0,0,1}, {0,1,0}, {0,1,0}
    };
    glColor3f(0,0,0);
    for (int f = 0; f < 6; ++f) {
        glPushMatrix();
        // 1. 先移動到面中心
        glTranslatef(label_pos_zh[f][0], label_pos_zh[f][1], label_pos_zh[f][2]);
        // 2. 讓文字面朝外且平貼在面上
        float nx = label_normal[f][0], ny = label_normal[f][1], nz = label_normal[f][2];
        float ux = label_up[f][0], uy = label_up[f][1], uz = label_up[f][2];
        // 計算旋轉矩陣
        float fx = uy * nz - uz * ny;
        float fy = uz * nx - ux * nz;
        float fz = ux * ny - uy * nx;
        float m[16] = {
            fx,  ux,  nx, 0,
            fy,  uy,  ny, 0,
            fz,  uz,  nz, 0,
            0,   0,   0,  1
        };
        glMultMatrixf(m);
        // 3. 可選：縮小字體，避免超出面
        glScalef(0.0015f, 0.0015f, 1.0f);
        // 4. 畫文字
        glRasterPos3f(0, 0, 0.01f); // 貼在面上
        const char* p = face_labels_zh[f];
        while (*p) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
            ++p;
        }
        glPopMatrix();
    }

    // --- 英文標籤 ---
    const char* face_labels[6] = {"R", "L", "U", "D", "F", "B"};
    float label_pos[6][3] = {
        {0.6f, 0.0f, 0.0f},   // +X 右 (Right)
        {-0.7f, 0.0f, 0.0f},  // -X 左 (Left)
        {0.0f, 0.6f, 0.0f},   // +Y 上 (Up)
        {0.0f, -0.7f, 0.0f},  // -Y 下 (Down)
        {0.0f, 0.0f, 0.6f},   // +Z 前 (Front)
        {0.0f, 0.0f, -0.7f}   // -Z 後 (Back)
    };
    glColor3f(0,0,0);
    for (int f = 0; f < 6; ++f) {
        glRasterPos3f(label_pos[f][0], label_pos[f][1], label_pos[f][2]);
        const char* p = face_labels[f];
        while (*p) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
            ++p;
        }
    }

    // 邊高亮
    static const int edge_indices[12][2] = {
        {0,1},{1,2},{2,3},{3,0}, // +Z 面
        {4,5},{5,6},{6,7},{7,4}, // -Z 面
        {0,4},{1,5},{2,6},{3,7}  // 側邊
    };
    static const float edge_vertices[8][3] = {
        {-0.5,-0.5, 0.5}, {0.5,-0.5, 0.5}, {0.5,0.5, 0.5}, {-0.5,0.5, 0.5},
        {-0.5,-0.5,-0.5}, {0.5,-0.5,-0.5}, {0.5,0.5,-0.5}, {-0.5,0.5,-0.5}
    };
    glLineWidth(4.0f);
    for (int i = 0; i < 12; ++i) {
        if (hover_type == 2 && hover_id == i)
            glColor3f(0.2, 1, 0.2); // 邊高亮
        else
            glColor3f(0.5, 0.5, 0.5);
        glBegin(GL_LINES);
        glVertex3fv(edge_vertices[edge_indices[i][0]]);
        glVertex3fv(edge_vertices[edge_indices[i][1]]);
        glEnd();
    }
    glLineWidth(1.0f);

    // 角高亮
    for (int i = 0; i < 8; ++i) {
        if (hover_type == 3 && hover_id == i)
            glColor3f(1, 0.2, 0.2); // 角高亮
        else
            glColor3f(0.8, 0.8, 0.8);
        glPointSize(10.0f);
        glBegin(GL_POINTS);
        glVertex3fv(edge_vertices[i]);
        glEnd();
    }
    glPointSize(1.0f);

    glPopMatrix();
    glViewport(0, 0, winWidth, winHeight);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int win_w = glutGet(GLUT_WINDOW_WIDTH);
    int win_h = glutGet(GLUT_WINDOW_HEIGHT);
    int size = 100;

    // 主視圖
    glViewport(0, 0, win_w, win_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, (float)win_w/win_h, 1, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0,0,8, 0,0,0, 0,1,0);

    glPushMatrix();
    glRotatef(view_rot_z, 0, 0, 1); // 新增
    glRotatef(view_rot_x, 1, 0, 0);
    glRotatef(view_rot_y, 0, 1, 0);
    glColor3f(1,1,1);
    drawCube(2.0);
    glPopMatrix();

    // ViewCube
    drawViewCube(win_w, win_h, size);

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    int win_w = glutGet(GLUT_WINDOW_WIDTH);
    int win_h = glutGet(GLUT_WINDOW_HEIGHT);
    int size = 100;
    y = win_h - y; // 修正座標
    checkViewCubeHover(x, y, win_w, win_h, size);

    // 新增：處理點擊 ViewCube cell
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (hover_type == 1 && hover_face >= 0 && hover_cell >= 0) {
            cube_rot_x = face_angles[hover_face][0];
            cube_rot_y = face_angles[hover_face][1];
            cube_rot_z = 0;
            view_rot_x = cube_rot_x;
            view_rot_y = cube_rot_y;
            view_rot_z = cube_rot_z;
            glutPostRedisplay();
            return;
        }
    }

    int vx = win_w - size - 10;
    int vy = 10;
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (x >= vx && x <= vx + size && y >= vy && y <= vy + size) {
            dragging = 1;
            dragging_main = 0;
            last_x = x;
            last_y = y;
            int mods = glutGetModifiers();
            drag_mode = (mods & GLUT_ACTIVE_SHIFT) ? 1 : 0;
        } else {
            dragging = 0;
            dragging_main = 1;
            last_x = x;
            last_y = y;
        }
    }
    if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        dragging = 0;
        dragging_main = 0;
        view_rot_x = cube_rot_x;
        view_rot_y = cube_rot_y;
    }
}

void motion(int x, int y) {
    int win_w = glutGet(GLUT_WINDOW_WIDTH);
    int win_h = glutGet(GLUT_WINDOW_HEIGHT);
    y = win_h - y; // 修正座標
    if (dragging) {
        if (drag_mode == 0) {
            // LMB: xy 旋轉
            cube_rot_y += (x - last_x);
            cube_rot_x += (y - last_y);
        } else {
            // Shift+LMB: z 軸旋轉
            cube_rot_z += (x - last_x); // 只用 x 拖曳控制 z 軸
        }
        last_x = x;
        last_y = y;
        // 讓主視圖同步
        view_rot_x = cube_rot_x;
        view_rot_y = cube_rot_y;
        view_rot_z = cube_rot_z;
        glutPostRedisplay();
    } else if (dragging_main) {
        // 主視圖拖曳
        view_rot_y += (x - last_x);
        view_rot_x += (y - last_y);
        last_x = x;
        last_y = y;
        // 讓 ViewCube 同步
        cube_rot_x = view_rot_x;
        cube_rot_y = view_rot_y;
        cube_rot_z = view_rot_z;
        glutPostRedisplay();
    }
}

void passiveMotion(int x, int y) {
    int win_w = glutGet(GLUT_WINDOW_WIDTH);
    int win_h = glutGet(GLUT_WINDOW_HEIGHT);
    int size = 100;
    y = win_h - y; // 修正座標
    int prev_type = hover_type, prev_id = hover_id;
    checkViewCubeHover(x, y, win_w, win_h, size);
    if (prev_type != hover_type || prev_id != hover_id) {
        glutPostRedisplay();
    }
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("3D View with ViewCube");
    glEnable(GL_DEPTH_TEST);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passiveMotion);

    glutMainLoop();
    return 0;
}