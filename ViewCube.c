#include <GL/glut.h>
#include <GL/glu.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float view_rot_x = 35.264f, view_rot_y = 45.0f, view_rot_z = 0;
float cube_rot_x = 35.264f, cube_rot_y = 45.0f, cube_rot_z = 0;
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
    {0.8,0.8,0.8},    // +X 灰
    {0.8,0.8,0.8},    // -X 灰
    {0.8,0.8,0.8},    // +Y 灰
    {0.8,0.8,0.8},    // -Y 灰
    {0.8,0.8,0.8},    // +Z 灰
    {0.8,0.8,0.8}     // -Z 灰
};

// --- 新增：細分面區域 ---
#define SUBDIV_CELLS 54  // 6面 x 9格
#define EDGE_DIV 4      // 分割點數量
#define FACE_SUBDIV_X 3
#define FACE_SUBDIV_Y 3
#define FACE_CELLS (FACE_SUBDIV_X * FACE_SUBDIV_Y)

// 定義分割比例
const float subdiv_ratios[4] = {
    0.0f,   // 開始
    0.1f,   // 1/10
    0.9f,   // 9/10
    1.0f    // 結束
};

// 儲存預計算的頂點
typedef struct {
    float vertices[4][3];  // 每個小面的四個頂點
} SubdivCell;

static SubdivCell subdivided_faces[SUBDIV_CELLS];

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

// 生成細分頂點
void generateSubdividedVertices() {
    for (int face = 0; face < 6; face++) {
        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                int cell_idx = face * 9 + y * 3 + x;
                float x1 = subdiv_ratios[x];
                float x2 = subdiv_ratios[x + 1];
                float y1 = subdiv_ratios[y];
                float y2 = subdiv_ratios[y + 1];
                
                lerp_face_vertex(face_vertices[face], x1, y1, subdivided_faces[cell_idx].vertices[0]);
                lerp_face_vertex(face_vertices[face], x2, y1, subdivided_faces[cell_idx].vertices[1]);
                lerp_face_vertex(face_vertices[face], x2, y2, subdivided_faces[cell_idx].vertices[2]);
                lerp_face_vertex(face_vertices[face], x1, y2, subdivided_faces[cell_idx].vertices[3]);
            }
        }
    }
}

// 繪製單一 cell
void drawFaceCell(int face, int cell, int highlight) {
    int cell_idx = face * 9 + cell;
    
    if (highlight)
        glColor3f(1, 0.8, 0.2);
    else
        glColor3fv(face_colors[face]);
        
    glBegin(GL_QUADS);
    for (int i = 0; i < 4; i++) {
        glVertex3fv(subdivided_faces[cell_idx].vertices[i]);
    }
    glEnd();
}

void drawCube(float size) {
    glutSolidCube(size);
}

// 輔助：將螢幕座標轉換為 ViewCube 內部的 3D 空間座標
void screenToViewCubeLocal(int x, int y, int winWidth, int winHeight, int size, float *out) {
    int vx = winWidth - size - 10;
    int vy = winHeight - size - 10;
    
    // 將鼠標位置轉換到 NDC 空間 [-1, 1]
    float fx = (2.0f * (x - vx) / (float)size) - 1.0f;
    float fy = (2.0f * (y - vy) / (float)size) - 1.0f;
    
    // 視點在 z = 5
    float eye_z = 5.0f;
    float near_z = 1.0f;
    float fov = 30.0f * M_PI / 180.0f;
    float tan_half_fov = tanf(fov * 0.5f);
    
    // 計算近平面上的點
    float near_x = fx * near_z * tan_half_fov;
    float near_y = fy * near_z * tan_half_fov;
    
    // 從視點(0,0,eye_z)到近平面上的點(near_x,near_y,near_z)形成射線
    float dx = near_x;
    float dy = near_y;
    float dz = near_z - eye_z;
    
    // 單位化射線方向
    float len = sqrtf(dx*dx + dy*dy + dz*dz);
    dx /= len;
    dy /= len;
    dz /= len;
    
    // 計算旋轉角度（與 drawViewCube 中的順序相反）
    float ry = -cube_rot_y * M_PI / 180.0f;  // Y軸
    float rx = -cube_rot_x * M_PI / 180.0f;  // X軸
    float rz = -cube_rot_z * M_PI / 180.0f;  // Z軸
    
    // 先旋轉 Y 軸
    float x1 = dx * cosf(ry) - dz * sinf(ry);
    float y1 = dy;
    float z1 = dx * sinf(ry) + dz * cosf(ry);
    
    // 再旋轉 X 軸
    float x2 = x1;
    float y2 = y1 * cosf(rx) - z1 * sinf(rx);
    float z2 = y1 * sinf(rx) + z1 * cosf(rx);
    
    // 最後旋轉 Z 軸
    float x3 = x2 * cosf(rz) - y2 * sinf(rz);
    float y3 = x2 * sinf(rz) + y2 * cosf(rz);
    float z3 = z2;
    
    // 輸出轉換後的方向向量
    out[0] = x3;
    out[1] = y3;
    out[2] = z3;
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
    float max_dot = -1.0f;
    int max_face = -1;
    float threshold = 0.3f; // 調整閾值，使檢測更寬鬆
    
    // 檢查與每個面的交點
    for (int i = 0; i < 6; ++i) {
        float nx = face_dirs[i][0];
        float ny = face_dirs[i][1];
        float nz = face_dirs[i][2];
        
        // 計算射線與面法向量的夾角余弦值
        float dot = lx * nx + ly * ny + lz * nz;
        
        // 只考慮朝向攝像機的面（dot < 0）
        if (dot < 0.0f && -dot > max_dot) {
            max_dot = -dot;
            max_face = i;
        }
    }
    
    // 如果沒有找到合適的面
    if (max_face < 0) {
        return;
    }

    // 計算面上的 UV 座標
    float u = 0.0f, vcell = 0.0f;
    if (max_face >= 0) {
        float scale = 1.0f / max_dot;  // 投影到面上的縮放因子
        
        switch (max_face) {
            case 0: // +X
                u = (-lz * scale + 0.5f);
                vcell = (ly * scale + 0.5f);
                break;
            case 1: // -X
                u = (lz * scale + 0.5f);
                vcell = (ly * scale + 0.5f);
                break;
            case 2: // +Y
                u = (lx * scale + 0.5f);
                vcell = (-lz * scale + 0.5f);
                break;
            case 3: // -Y
                u = (lx * scale + 0.5f);
                vcell = (lz * scale + 0.5f);
                break;
            case 4: // +Z
                u = (lx * scale + 0.5f);
                vcell = (ly * scale + 0.5f);
                break;
            case 5: // -Z
                u = (-lx * scale + 0.5f);
                vcell = (ly * scale + 0.5f);
                break;
        }
    }

    // 判斷 cell 位置
    int cell_x = -1, cell_y = -1;
    // 檢查是否在有效範圍內（加入容差）
    float tolerance = 0.05f;  // 增加容差值
    if (u >= -tolerance && u <= 1.0f + tolerance &&
        vcell >= -tolerance && vcell <= 1.0f + tolerance) {
        
        // 限制在有效範圍內
        u = fmaxf(0.0f, fminf(1.0f, u));
        vcell = fmaxf(0.0f, fminf(1.0f, vcell));
        
        // 判斷 cell_x
        for (int i = 0; i < FACE_SUBDIV_X; ++i) {
            float x1 = subdiv_ratios[i];
            float x2 = subdiv_ratios[i+1];
            if (u >= x1 - tolerance && u <= x2 + tolerance) {
                cell_x = i;
                break;
            }
        }
        // 判斷 cell_y
        for (int i = 0; i < FACE_SUBDIV_Y; ++i) {
            float y1 = subdiv_ratios[i];
            float y2 = subdiv_ratios[i+1];
            if (vcell >= y1 - tolerance && vcell <= y2 + tolerance) {
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
    }
    
    // 如果沒有找到合適的 cell
    hover_type = 0;
    hover_id = -1;

    // 其他類型的 hover 檢測（邊和角）可以在這裡添加
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
    
    int prev_hover_face = hover_face;
    int prev_hover_cell = hover_cell;
    int prev_hover_type = hover_type;
    
    checkViewCubeHover(x, y, win_w, win_h, size);
    
    if (prev_hover_face != hover_face || 
        prev_hover_cell != hover_cell || 
        prev_hover_type != hover_type) {
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
    
    generateSubdividedVertices();  // 初始化細分頂點

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passiveMotion);

    glutMainLoop();
    return 0;
}