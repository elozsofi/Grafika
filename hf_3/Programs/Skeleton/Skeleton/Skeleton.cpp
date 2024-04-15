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
//					- Moodle: elõadás diák az openGL metódusokkhoz, fragment és vertex shaderekhez, Camera felépítésében
//					- Teams: nem hivatalos grafika konzi, ahol az elméleti alapok lettek átismételve,
//							 a program (nem konkrét) felépítésében adtak tanácsokat, segítettek pontosítani a feladatot,
//							 levezetni a fontosabb képleteket. 
//							 Ezt felhasználtam a Star osztály pontjainak koordinátáit kiszámoló fgvben (getPoints),
//							 a Poincaré textúrát képezõ ,,saktábla" meghatározásában (getCheckerBoard), és a Camera transzformációinak megírásában
//=============================================================================================
#include "framework.h"

/*
 * pontokon kívül a textúra koordinátákat is vinni a pipelineon
 * pixelárnyaló az interpolált textúra koordinátákkal megcímzett texellel színezze a pixelt
 */
// vertex shader in GLSL
// "2D textúrázás" elõadás diáiból
const char* const vertexSource = R"(
	#version 330
	precision highp float;
	uniform mat4 MVP;
	layout(location = 0) in vec3 vp;	
	layout(location = 1) in vec2 vertexUV;
	out vec2 texCoord;
	void main() {
		texCoord = vertexUV;
		gl_Position = vec4(vp.x, vp.y, vp.z, 1) * MVP;
	}
)";

// fragment shader in GLSL
const char* const fragmentSource = R"(
	#version 330
	precision highp float;
	uniform sampler2D samplerUnit;
	in vec2 texCoord;
	out vec4 fragmentColor;
	void main() {
		fragmentColor = texture(samplerUnit, texCoord);
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders

// 2D camera
struct Camera {
	mat4 P, V, MVP;
	Camera(){
		P = ScaleMatrix(vec3((2 / 150.0f), (2 / 150.0f), 1));
		V = TranslateMatrix(vec3(-1 * 20, -1 * 30, 0));
		MVP = V * P;
		getUniform();
	}

	void getUniform() {
		int location = glGetUniformLocation(gpuProgram.getId(), "MVP");
		glUniformMatrix4fv(location, 1, GL_TRUE, &MVP[0][0]);
	}

	void animate(float f) {
		MVP = (TranslateMatrix(vec3(-50, -30, 0))) * (RotationMatrix(f, vec3(0, 0, 1))) * (TranslateMatrix(vec3(30, 0, 0))) * (RotationMatrix(f, vec3(0, 0, 1))) * (TranslateMatrix(vec3(20, 30, 0))) * V * P;
		getUniform();
	}
};

class PoincareTexture {
public:
	/*
	 * RenderToTexture : aktuális felbontástól függõen
	 * számít textúra képet + feltölt GPU-ba
	 * A textúra kezdeti felbontása 300x300-as és GL_LINEAR a szûrési mód
	 */
	GLuint textureId;
	GLenum filterMode;
	int resolution; // default 300x300
	std::vector<vec4> image;
	std::vector<vec4> circles;
	std::vector<vec3> points;

	PoincareTexture() : resolution(300), filterMode(GL_LINEAR){
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		getCheckerBoard();
		fillTextureImage();
		draw();
	}

	void draw() {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resolution, resolution, 0, GL_RGBA, GL_FLOAT, &image[0]);
		setTextureParameters();
		glBindTexture(GL_TEXTURE_2D, textureId);
	}
	
	void setTextureParameters(){
		const GLenum textureOptions[4] = {
			GL_TEXTURE_MIN_FILTER,
			GL_TEXTURE_MAG_FILTER,
			GL_TEXTURE_WRAP_S,
			GL_TEXTURE_WRAP_T
		};
		for (GLenum option : textureOptions) {
			glTexParameteri(GL_TEXTURE_2D, option, filterMode);
		}
	}

	void getCheckerBoard() {
		circles.clear();
		for (int i = 0; i < 360; i += 40) {
			vec2 v0(cosf((i * ((float)M_PI / 180.0f))), sinf((i * ((float)M_PI / 180.0f))));

			// feladatleírásból pontok koordinátái
			points.clear();
			for (float f = 0.5f; f < 6.5f; f++) {
				vec3 hp = vec3(0, 0, 1) * coshf(f) + (vec3(v0.x,v0.y,0)) * sinhf(f);
				vec3 pp(hp.x / (hp.z + 1), hp.y / (hp.z + 1), 0);
				points.push_back(pp);
			}

			for (const auto& j : points) {
				float oqabs = 1 / ((length(j)));
				vec3 Q = vec3(v0.x,v0.y,0) * oqabs;
				vec3 center((j.x + Q.x) / 2, (j.y + Q.y) / 2, 0);
				circles.push_back(vec4(center.x, center.y, center.z, (length((Q - j)) / 2)));
			}
		}
	}

	vec4 calculatePixelColor(int i, int j) {
		float x = static_cast<float>(j) / (resolution / 2);
		float y = static_cast<float>(i) / (resolution / 2);

		if (std::pow(x, 2) + std::pow(y, 2) > 1) {
			return vec4(0, 0, 0, 1);
		}

		int colorHelper = 0;
		for (const auto& circle : circles) {
			float distSquared = std::pow(x - circle.x, 2) + std::pow(y - circle.y, 2);
			if (distSquared > std::pow(circle.w, 2)) {
				colorHelper++;
			}
		}
		return (colorHelper % 2 == 0) ? vec4(1, 1, 0, 1) : vec4(0, 0, 1, 1);
	}

	void fillTextureImage() {
		image.clear();
		for (int i = -resolution / 2; i < resolution / 2; i++) {
			for (int j = -resolution / 2; j < resolution / 2; j++) {
				vec4 color = calculatePixelColor(i, j);
				image.push_back(color);
			}
		}
	}

	void setFilter(GLenum f) {
		filterMode = f;		
		fillTextureImage();
		draw();
	}

	void changeResolution(int i) {
		resolution += i;
		fillTextureImage();
		draw();
	}

	int getResolution() {
		return resolution;
	}

	GLuint getTextureId() const {
		return textureId;
	}

	const char* getFilter() {
		return (filterMode == GL_LINEAR) ? "GL_LINEAR" : "GL_NEAREST";
	}
};

	/*
	 * csillag hegyesítés
	 * keringés, forgás
	 * geometria textúrázás
	 */
class Star {
public:
	GLuint vao, vbo, vboTex;
	bool animation;
	float centerX;
	float centerY;
	float radius;
	long animationTimer;
	std::vector<vec3> vtx;
	std::vector<vec2> texture;

	Star(float cx = 50, float cy = 30, float r = 40, bool a = false)
		: centerX(cx), centerY(cy), radius(r), animation(a), animationTimer(0){
		initialize();
	}

	void calculateVertices() {
		vtx.clear();
		texture.clear();
		std::vector<vec3> cp;
		std::vector<vec3> mp;
		for (int i = -1; i < 2; ++i) {
			for (int j = -1; j < 2; ++j) {
				if (i == 0 && j == 0) continue;

				float x = centerX + (i * 40);
				float y = centerY + (j * 40);

				if (i == 0 || j == 0) {
					x = centerX + (i * radius);
					y = centerY + (j * radius);
				}

				vec3 point(x, y, 1);

				if (i == 0 || j == 0) {
					mp.push_back(point);
				}
				else {
					cp.push_back(point);
				}
			}
		}
		// feladatleírásból
		const std::vector<vec2> texelPoints = { vec2(0.5f, 0.5f) , vec2(0.0f, 0.0f) , vec2(0.0f, 0.5f) , vec2(0, 1), vec2(0.5f, 1.0f), vec2(1.0f, 1.0f), vec2(1.0f, 0.5f),  vec2(1, 0) , vec2(0.5f, 0.0f) , vec2(0.0f, 0.0f) };
		std::vector<vec3> vtxPoints = { vec3(centerX, centerY, 1) , cp[0] , mp[0], cp[1], mp[2],  cp[3] , mp[3] ,cp[2] , mp[1] , cp[0] };
		for (int i = 0; i < 10; i++) {
			vtx.push_back(vtxPoints[i]);
			texture.push_back(texelPoints[i]);
		}
	}
	void initialize() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(0);

		glGenBuffers(1, &vboTex);
		glBindBuffer(GL_ARRAY_BUFFER, vboTex);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(1);

		calculateVertices();
		updateBuffers();
	}

	void updateBuffers() {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec3), vtx.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, vboTex);
		glBufferData(GL_ARRAY_BUFFER, texture.size() * sizeof(vec2), texture.data(), GL_DYNAMIC_DRAW);
	}

	void update() {
		calculateVertices();
		updateBuffers();
		glDrawArrays(GL_TRIANGLE_FAN, 0, vtx.size()); // Render the star
	}

	void changeShape(float f) {
		radius += f;
		update();
	}

	float getRadius() {
		return radius;
	}

	void setAnimation(bool b) {
		animationTimer = glutGet(GLUT_ELAPSED_TIME);
		animation = b;
	}

	long getanimationTimer() {
		return animationTimer;
	}
	bool getAnimation() {
		return animation;
	}
};

Camera* camera;
PoincareTexture* pt;
Star* star;

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
	pt = new PoincareTexture();
	star = new Star();
	camera = new Camera();
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);							// background color 
	glClear(GL_COLOR_BUFFER_BIT); // clear the screen
	if (star != nullptr) {
		star->update();
	}
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key) {
	case 'r': pt->changeResolution(-100); break;
	case 'R': pt->changeResolution(100); break;
	case 't': pt->setFilter(GL_NEAREST); break;
	case 'T': pt->setFilter(GL_LINEAR); break;
	case 'H': star->changeShape(10); break;
	case 'h': star->changeShape(-10); break;
	case 'a': printf("animacio\n");  star->setAnimation(!(star->getAnimation())); break;
	}
	printf("%s, %d, %.2f\n", pt->getFilter(), pt->getResolution(), star->getRadius());
	glutPostRedisplay();        // redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	if (star->getAnimation()) {
		long time = glutGet(GLUT_ELAPSED_TIME)-star->getanimationTimer();
		camera->animate((float)(time)*((float)M_PI/2000.0f));
	}
	glutPostRedisplay();	
}
