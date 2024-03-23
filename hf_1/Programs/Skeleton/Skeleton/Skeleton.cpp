//=============================================================================================
// Mintaprogram: Zöld háromszög. Ervenyes 2019. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : 	
// Neptun : 
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL
const char* vertexSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 vertexPosition;
	void main() {
		gl_Position = vec4(vertexPosition, 0, 1); // in NDC
	}
)";

// fragment shader in GLSL
const char* fragmentSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;
	out vec4 fragmentColor;
	void main() {
		fragmentColor = vec4(color, 1);
	}
)";

GPUProgram gpuProgram;	// vertex and fragment shaders

typedef enum State {
	none,
	point,
	line,
	lineMove,
	linePoint
} State;

class Object {
	unsigned int vao, vbo; // GPU
	std::vector<vec3> vtx; // CPU
public:
	// cpu-n tárolt gyûjtemény megváltoztatása
	// gpu reprezentáció szinkronba hozása az aktuális cpu reprezentációval
	// gpu reprezentáció felrajzolása a megadott primitív típussal és színnel
	Object() {
		glGenVertexArrays(1, &vao); glBindVertexArray(vao);
		glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	std::vector<vec3>& Vtx() { return vtx; }

	void updateGPU() { // CPU -> GPU
		glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec3), &vtx[0], GL_DYNAMIC_DRAW);
	}
	void Draw(int type, vec3 color) {
		if (vtx.size() > 0) {
			glBindVertexArray(vao);
			gpuProgram.setUniform(color, "color");
			glDrawArrays(type, 0, vtx.size());
		}
	}
};

class PointCollection {
	Object			points;
public:
	// új pont hozzávétele
	// pont közelségi keresése
	// pontok felrajzolása a kapott színnel
	void addPoint(float cX, float cY) {
		vec3 newPoint = vec3(cX, cY, 1);
		points.Vtx().push_back(newPoint);
		printf("Point %.1f, %.1f added\n", cX, cY);
	}

	void update() {
		if (points.Vtx().size() > 0)
			points.updateGPU();
	}

	vec3* pickPoint(vec2 pp, float threshold) {
		for (auto& p : points.Vtx()) {
			if (length(vec2(p.x, p.y) - pp) < threshold) return &p;
		}
		return nullptr;
	}

	void Draw() {
		points.Draw(GL_POINTS, vec3(1, 0, 0));
	}
};

PointCollection* points;

class Line {
public:
	vec3 start, end;
	vec2 normVec;
	vec2 parVec;
	// konstruálás két pontból <- implicit egyenlet és paraméteres egyenlet konzolra írása
	// két egyenes metszéspontjának meghatározása
	// annak eldöntése, hogy egy pont rajta van-e az egyenesen
	// egyenes azon szakaszának meghatározása, amely a (-1,1) és (1,1) sarokpontú négyzet belsejébe esõ részt lefedi
	// egyenes eltolása úgy, hogy a kapott ponton átmenjen
	Line(vec3 start, vec3 end) : start(start), end(end) {
		float A = end.y - start.y;
		float B = start.x - end.x;
		float C = end.x * start.y - start.x * end.y;

		float dx = end.x - start.x;
		float dy = end.y - start.y;

		parVec = vec2(end.x, end.y) - vec2(start.x, start.y);
		normVec = vec2(parVec.y * (-1), parVec.x);
		printf("Line added\n\tImplicit: %.1f x + %.1fy + %.1f = 0\n\tParametric: r(t) = (%.1f, %.1f) + (%.1f, %.1f)t\n", A, B, C, start.x, start.y, dx, dy);
	}

	static vec2 intersection(const Line& line1, const Line& line2) {
		float A1 = line1.end.y - line1.start.y;
		float B1 = line1.start.x - line1.end.x;
		float C1 = A1 * line1.start.x + B1 * line1.start.y;

		float A2 = line2.end.y - line2.start.y;
		float B2 = line2.start.x - line2.end.x;
		float C2 = A2 * line2.start.x + B2 * line2.start.y;

		return vec2(((-1) * (B2 * C1 - B1 * C2) / (A2 * B1 - A1 * B2)), (-1) * ((A2 * C1 - A1 * C2) / (A1 * B2 - A2 * B1)));
	}

	float distanceFromPoint(vec2 point) {
		float A = end.y - start.y;
		float B = start.x - end.x;
		float C = end.x * start.y - start.x * end.y;
		return A * point.x + B * point.y + C;
	}

	void Draw() {
		vec3 direction = normalize(end - start);
		float ext = 2.0f;
		vec3 extStart = start - direction * ext;
		vec3 extEnd = end + direction * ext;
		std::vector<vec3> lineVertices = { extStart, extEnd };
		Object lineObject;
		lineObject.Vtx() = lineVertices;
		lineObject.updateGPU();
		lineObject.Draw(GL_LINES, vec3(0, 1, 1));
	}

	void move(vec2 newStart) {
		vec2 displacement = newStart - vec2(start.x, start.y);
		start = vec3(newStart.x, newStart.y, 0);
		end = vec3(end.x + displacement.x, end.y + displacement.y, 0);
	}
};

class LineCollection {
	std::vector<Line>	lines;
public:
	Line* selectedLines[2] = { nullptr, nullptr };
	int numSelLines = 0;
	Line* selectedLine = nullptr;
	// egyenes hozzáadása
	// egy pont koordinátái alapján egy közeli kiválasztása
	// egyenesek felrajzolása a kapott színnel
	void addLine(Line line) {
		lines.push_back(line);
	}

	void Draw() {
		for (auto& line : lines) {
			line.Draw();
		}
	}

	Line* pickLine(vec2 mousePos) {
		Line* closestLine = nullptr;

		for (auto& line : lines) {
			float distance = line.distanceFromPoint(mousePos);
			if (distance <= 0.01f && distance >= -0.01f) {
				closestLine = &line;
			}
		}
		if (closestLine) {
			if (numSelLines == 2) {
				selectedLines[0] = nullptr;
				selectedLines[1] = nullptr;
				numSelLines = 0;
			}
			selectedLine = closestLine;
			selectedLines[numSelLines++] = closestLine;
		}
		return closestLine;
	}

	void addIntersection() {
		if (numSelLines == 2) {
			vec2 newPoint = Line::intersection(*selectedLines[0], *selectedLines[1]);
			if ((newPoint.x >= -1 && newPoint.x <= 1) && (newPoint.y <= 1 && newPoint.y >= -1)) {
				points->addPoint(newPoint.x, newPoint.y);
				points->update();
				numSelLines = 0;
			}
		}
	}

	void moveSelLine(vec2 mousePos) {
		Line* closestLine = pickLine(mousePos);
		if (selectedLine) {
			selectedLine->move(mousePos);
		}
	}
};


// The virtual world: collection of objects
LineCollection* lines;
vec2* pickedPoint = nullptr;
State					state;
vec3					selectedPoint1, selectedPoint2;
bool isPointSelected = false;

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight); 	// Position and size of the photograph on screen
	glLineWidth(3); // Width of lines in pixels
	glPointSize(10);

	points = new PointCollection;
	lines = new LineCollection;

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "fragmentColor");

	state = none;
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.3, 0.3, 0.3, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	lines->Draw();
	points->Draw();
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key) {
	case 'p': state = point;  printf("most pontokat rakhatsz le\n"); break;
	case 'l': state = line; printf("most ket pontra illeszkedo egyenest rakhatsz le\n"); break;
	case 'm': state = lineMove;  printf("most elmozgathatsz egy egyenest\n"); break;
	case 'i': 
		state = linePoint; 
		printf("most ket egyenes metszespontjara rakhatsz 1 pontot\n");
		lines->selectedLines[0] = nullptr;
		lines->selectedLines[1] = nullptr;
		lines->numSelLines = 0; break;
	}
	glutPostRedisplay();
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}


// Mouse click event
void onMouse(int button, int mouseState, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON && mouseState == GLUT_DOWN) {
		float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
		float cY = 1.0f - 2.0f * pY / windowHeight;
		vec2 mousePos = vec2(cX, cY);

		switch (state) {
		case point:

			points->addPoint(cX, cY);	// addpoint
			points->update();			// update
			glutPostRedisplay();		// redraw
			break;
		case line: {
			vec3* pickedPoint = points->pickPoint(mousePos, 0.05f);
			if (pickedPoint) {
				if (!isPointSelected) {
					selectedPoint1 = *pickedPoint;
					isPointSelected = true;
				}
				else {
					selectedPoint2 = *pickedPoint;
					if (!(selectedPoint1.x == selectedPoint2.x && selectedPoint1.y == selectedPoint2.y && selectedPoint1.z == selectedPoint2.z)) {
						lines->addLine(Line(selectedPoint1, selectedPoint2));
					}
					isPointSelected = false;
					glutPostRedisplay();
				}
			}
		} break;
		case lineMove: {
			lines->moveSelLine(mousePos);
			glutPostRedisplay();
		} break;
		case linePoint: {
			Line* selLine = lines->pickLine(mousePos);
			if (selLine && lines->numSelLines == 2) {
				if (lines->numSelLines == 2) {
					lines->addIntersection();
					glutPostRedisplay();
					lines->numSelLines = 0;
				}
			}
		} break;
		case none: {} break;
		}
	}
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
	if (state == lineMove && lines->selectedLine != nullptr) {
		vec2 mousePos = { 2.0f * pX / windowWidth - 1, 1.0f - 2.0f * pY / windowHeight };
		if (mousePos.x >= -1 && mousePos.x <= 1 && mousePos.y >= -1 && mousePos.y <= 1) {
			lines->moveSelLine(mousePos); // Egyenes mozgatása
			glutPostRedisplay(); // A változások megjelenítése
		}
	}
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
}