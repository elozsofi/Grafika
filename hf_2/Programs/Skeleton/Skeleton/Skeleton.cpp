//=============================================================================================
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
//
// A kód írásához az alábbi forrásokat használtam fel:
//			- tárgy moodle oldalán található elõadás diák 
//					-> Curve osztály, Lagrange, Bezier, Catmull-Rom görbék kódja megtalálható volt bennük, a képleteket kellett kiegészíteni
//					-> Camera osztály mátrixai a moodle-re kitett példakódból
//			- tárgy honlapjára kitett példakódok, videók: cg.iit.bme.hu/portal/oktatott-targyak/szamitogepes-grafika-es-kepfeldolgozas/geometriai-modellezes
//					-> ezen belül felhasználtam a "Görbe szerkesztõ" videót, ami a CatmullRomSpline osztály egy részét inspirálta
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL
const char* vertexSource = R"(
	#version 330
    precision highp float;

	uniform mat4 MVP;			// Model-View-Projection matrix in row-major format

	layout(location = 0) in vec2 vertexPosition;	// Attrib Array 0

	void main() {
		gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 		// transform to clipping space
	}
)";

// fragment shader in GLSL
const char* fragmentSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

	void main() {
		fragmentColor = vec4(color, 1); // extend RGB to RGBA
	}
)";

// 2D camera
struct Camera {
	vec2 wCenter;
	vec2 wSize;
public:
	Camera() {
		wCenter = vec2(0, 0); wSize = vec2(30, 30);
	}

	mat4 V() { // view matrix
		return TranslateMatrix(-wCenter);
	}

	mat4 P() { // projection matrix
		return ScaleMatrix(vec2(2 / wSize.x, 2 / wSize.y));
	}

	mat4 Vinv() { // inverse view matrix
		return TranslateMatrix(wCenter);
	}

	mat4 Pinv() { // inverse projection matrix
		return ScaleMatrix(vec2(wSize.x / 2, wSize.y / 2));
	}

	void Pan(vec2 m) { // move camera
		wCenter = wCenter + m;
	}

	void Zoom(float f) { // zoom-in/out by f
		wSize = wSize * f;
	}
};

// state for choosing which curve to draw
typedef enum State {
	none,
	Bezier,
	Lagrange,
	Catmull_Rom
}State;

Camera camera;	// 2D camera
GPUProgram gpuProgram; // vertex and fragment shaders
int nTesselatedVertices = 100;

class Curve {
	unsigned int vaoCurve, vboCurve;
	unsigned int vaoCtrlPoints, vboCtrlPoints;
protected:
	std::vector<vec2> wCtrlPoints;		// coordinates of control points
public:
	Curve() {
		// Curve
		glGenVertexArrays(1, &vaoCurve);
		glBindVertexArray(vaoCurve);
		glGenBuffers(1, &vboCurve); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset

		// Control Points
		glGenVertexArrays(1, &vaoCtrlPoints);
		glBindVertexArray(vaoCtrlPoints);
		glGenBuffers(1, &vboCtrlPoints); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0
		// Map attribute array 0 to the vertex data of the interleaved vbo
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), NULL); // attribute array, components/attribute, component type, normalize?, stride, offset
	}

	~Curve() {
		glDeleteBuffers(1, &vboCtrlPoints);
		glDeleteVertexArrays(1, &vaoCtrlPoints);
		glDeleteBuffers(1, &vboCurve);
		glDeleteVertexArrays(1, &vaoCurve);
	}

	virtual vec2 r(float t) = 0;
	virtual float tStart() = 0;
	virtual float tEnd() = 0;

	virtual void AddControlPoint(float cX, float cY) {
		vec4 wVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		wCtrlPoints.push_back(vec2(wVertex.x, wVertex.y));
	}

	// Returns the selected control point or -1
	int PickControlPoint(float cX, float cY) {
		vec4 hVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		vec2 wVertex = vec2(hVertex.x, hVertex.y);
		for (unsigned int p = 0; p < wCtrlPoints.size(); p++) {
			if (dot(wCtrlPoints[p] - wVertex, wCtrlPoints[p] - wVertex) < 0.1) return p;
		}
		return -1;
	}

	void MoveControlPoint(int p, float cX, float cY) {
		vec4 hVertex = vec4(cX, cY, 0, 1) * camera.Pinv() * camera.Vinv();
		wCtrlPoints[p] = vec2(hVertex.x, hVertex.y);
	}

	void Draw() {
		mat4 VPTransform = camera.V() * camera.P();
		gpuProgram.setUniform(VPTransform, "MVP");

		if (wCtrlPoints.size() > 0) {	// draw control points
			glBindVertexArray(vaoCtrlPoints);
			glBindBuffer(GL_ARRAY_BUFFER, vboCtrlPoints);
			glBufferData(GL_ARRAY_BUFFER, wCtrlPoints.size() * sizeof(vec2), &wCtrlPoints[0], GL_DYNAMIC_DRAW);
			gpuProgram.setUniform(vec3(1, 0, 0), "color");
			glPointSize(10.0f);
			glDrawArrays(GL_POINTS, 0, wCtrlPoints.size());
		}
		if (wCtrlPoints.size() >= 2) {	// draw curve
			std::vector<vec2> vertexData;
			for (int i = 0; i < nTesselatedVertices; i++) {	// Tessellate
				float tNormalized = (float)i / (nTesselatedVertices - 1);
				float t = tStart() + (tEnd() - tStart()) * tNormalized;
				vec2 wVertex = r(t);
				vertexData.push_back(wVertex);
			}
			// copy data to the GPU
			glBindVertexArray(vaoCurve);
			glBindBuffer(GL_ARRAY_BUFFER, vboCurve);
			glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(vec2), &vertexData[0], GL_DYNAMIC_DRAW);
			gpuProgram.setUniform(vec3(1, 1, 0), "color");
			glLineWidth(2.0f);
			glDrawArrays(GL_LINE_STRIP, 0, nTesselatedVertices);
		}
	}
};

// Bezier curve using Bernstein polynomials
class BezierCurve : public Curve {
	float B(int i, float t) {
		int n = wCtrlPoints.size() - 1;
		float choose = 1;
		for (int j = 1; j <= i; j++) choose *= (float)(n - j + 1) / j;
		return choose * pow(t, i) * pow(1 - t, n - i);
	}
public:
	float tStart() { return 0; }
	float tEnd() { return 1; }
	virtual vec2 r(float t) {
		vec2 wPoint(0, 0);
		for (unsigned int n = 0; n < wCtrlPoints.size(); n++) {
			wPoint = wPoint + wCtrlPoints[n] * B(n, t);
		}
		return wPoint;
	}
};

// Lagrange curve
class LagrangeCurve : public Curve {
	std::vector<float> ts;  // knots
	float L(int i, float t) {
		float Li = 1.0f;
		for (unsigned int j = 0; j < wCtrlPoints.size(); j++)
			if (j != i) {
				Li *= (t - ts[j]) / (ts[i] - ts[j]);
			}
		return Li;
	}
public:
	void AddControlPoint(float cx, float cy) {
		float newDistance = 0.0f;
		float totalLength = 0.0f;
		float len = 0.0f;
		float distanceBetween;

		for (auto f = ts.begin(); f != ts.end();) { // uj pontnal mindig ujra kell szamolni az osszes csomoerteket
			f = ts.erase(f);
		}; 

		ts.push_back(0.0f); //elso csomoertek mindig 0, ezutan az alabbi keplet szerint kell kiszamolni

		Curve::AddControlPoint(cx, cy);

		// teljes gorbe hossz + 2 legutobbi pont kozti tav kiszamolasa
		if (wCtrlPoints.size() > 1) {
			for (int i = 1; i < wCtrlPoints.size(); i++) {
				newDistance = length(wCtrlPoints[i] - wCtrlPoints[i - 1]);
				totalLength = totalLength + newDistance;
			}

			for (int j = 1; j < wCtrlPoints.size(); j++) {
				len = 0.0f;
				for (int k = 1; k <= j; k++) {
					distanceBetween = length(wCtrlPoints[k] - wCtrlPoints[k - 1]);
					len = len + distanceBetween;
				}
				float newPoint = len / totalLength;
				ts.push_back(newPoint);
			}
		}

		printf("Lagrange csomoertekek: ");
		for (size_t i = 0; i < ts.size(); ++i) {
			printf("%.2f\t", ts[i]);
		}
		printf("\n");
	}
	float tStart() { return ts[0]; }
	float tEnd() { return ts[wCtrlPoints.size() - 1]; }

	virtual vec2 r(float t) {
		vec2 wPoint(0, 0);
		for (int n = 0; n < wCtrlPoints.size(); n++) {
			wPoint = wPoint + wCtrlPoints[n] * L(n, t);
		}
		return wPoint;
	}
};

class CatmullRomSpline : public Curve {
	std::vector<float> ts; // parameter (knot) values

	vec2 Hermite(vec2 p0, vec2 v0, float t0, vec2 p1, vec2 v1, float t1, float t) {
		float dt = t1 - t0;
		t = t - t0;
		float dt2 = dt * dt;
		float dt3 = dt * dt2;
		vec2 a0 = p0;
		vec2 a1 = v0;
		vec2 a2 = (p1 - p0) * 3 / dt2 - (v1 + v0 * 2) / dt;
		vec2 a3 = (p0 - p1) * 2 / dt3 + (v1 + v0) / dt2;
		return ((a3 * t + a2) * t + a1) * t + a0;
	}
public:
	float tension = 0.0f;
	void AddControlPoint(float cx, float cy) {
		float distance = 0.0f;
		float totalLength = 0.0f;
		float distanceBetween;

		ts.clear();
		ts.push_back(0.0f);

		Curve::AddControlPoint(cx, cy);

		// teljes gorbe hossz + 2 legutobbi pont kozti tav kiszamolasa
		if (wCtrlPoints.size() > 1) {
			for (int i = 1; i < wCtrlPoints.size(); i++) {
				distance = length(wCtrlPoints[i] - wCtrlPoints[i - 1]);
				totalLength = totalLength + distance;
			}

			for (int j = 1; j < wCtrlPoints.size(); j++) {
				float len = 0.0f;
				for (int k = 1; k <= j; k++) {
					distanceBetween = length(wCtrlPoints[k] - wCtrlPoints[k - 1]);
					len += distanceBetween;
				}
				float newPoint = len / totalLength;
				ts.push_back(newPoint);
			}
		}

		printf("Catmull-Rom spline csomoertekek: ");
		for (size_t i = 0; i < ts.size(); ++i) {
			printf("%.2f\t", ts[i]);
		}
		printf("\n");
	}
	float tStart() { return ts[0]; }
	float tEnd() { return ts[wCtrlPoints.size() - 1]; }

	void Tension(float t) {
		tension = t;
	}

	virtual vec2 r(float t) {
		vec2 wPoint(0, 0);
		for (int i = 0; i < wCtrlPoints.size() - 1; i++) {
			if (ts[i] <= t && t <= ts[i + 1]) {
				vec2 vPrev = (i > 0) ? (wCtrlPoints[i] - wCtrlPoints[i - 1]) * (1.0f / (ts[i] - ts[i - 1])) : vec2(0, 0);
				vec2 vCur = (wCtrlPoints[i + 1] - wCtrlPoints[i]) / (ts[i + 1] - ts[i]);
				vec2 vNext = (i < wCtrlPoints.size() - 2) ? (wCtrlPoints[i + 2] - wCtrlPoints[i + 1]) * (1.0f / (ts[i + 2] - ts[i + 1])) : vec2(0, 0);
				vec2 v0 = (vPrev + vCur) * 0.5f * (1.0f - tension);
				vec2 v1 = (vCur + vNext) * 0.5f * (1.0f - tension);

				return Hermite(wCtrlPoints[i], v0, ts[i], wCtrlPoints[i + 1], v1, ts[i + 1], t);
			}
		}
		return wCtrlPoints[0];
	}

};

// The virtual world: collection of two objects
Curve* curve;
State state;

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	glLineWidth(2.0f);
	glPointSize(10);
	curve = nullptr;
	state = none;
	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	if (curve) {
		curve->Draw();
	}
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key) {
	case 'P': printf("kamera jobbra\n"); camera.Pan(vec2(1, 0)); break;
	case 'p': printf("kamera balra\n"); camera.Pan(vec2(-1, 0)); break;
	case 'Z': printf("zoom-out\n"); camera.Zoom(1.1); break;
	case 'z': printf("zoom-in\n"); camera.Zoom(1 / 1.1); break;
	case 'l': state = Lagrange; printf("Lagrange\n"); curve = new LagrangeCurve(); break;
	case 'b': state = Bezier;  printf("Bezier\n"); curve = new BezierCurve(); break;
	case 'c': state = Catmull_Rom;  printf("Catmull-Rom\n"); curve = new CatmullRomSpline();  break;
	case 'T':
		if (state == Catmull_Rom) {
			CatmullRomSpline* crSpline = static_cast<CatmullRomSpline*>(curve);
			float newTension = crSpline->tension + 0.1f;
			crSpline->Tension(newTension);
			printf("Catmull-Rom tenzio = %f\n", newTension);
			glutPostRedisplay();
		}
		break;
	case 't':
		if (state == Catmull_Rom) {
			CatmullRomSpline* crSpline = static_cast<CatmullRomSpline*>(curve);
			float newTension = crSpline->tension - 0.1f;
			crSpline->Tension(newTension);
			printf("Catmull-Rom tenzio = %f\n", newTension);
			glutPostRedisplay();
		}
		break;
	}
	glutPostRedisplay();        // redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {}

int pickedControlPoint = -1;

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		float cX = 2.0f * pX / windowWidth - 1;
		float cY = 1.0f - 2.0f * pY / windowHeight;
		if (curve != nullptr) {
			curve->AddControlPoint(cX, cY);
			glutPostRedisplay();
		}
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
		float cX = 2.0f * pX / windowWidth - 1;
		float cY = 1.0f - 2.0f * pY / windowHeight;
		if (curve != nullptr) {
			pickedControlPoint = curve->PickControlPoint(cX, cY);
			glutPostRedisplay();
		}
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
		pickedControlPoint = -1;
	}
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
	float cX = 2.0f * pX / windowWidth - 1;	// flip y axis
	float cY = 1.0f - 2.0f * pY / windowHeight;
	if (pickedControlPoint >= 0) curve->MoveControlPoint(pickedControlPoint, cX, cY);
	glutPostRedisplay();
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {}