#include <iostream>
#include <random>
#include <sstream>
#include <fstream>
#include <cmath>
#include <chrono>
#include <string>
#include <cstdio>
#include "allegro/allegro.h"

#define N_DINOS 30

using namespace std;

SpriteMap spritemap;
vector<Sprite> cactus_sprites;
vector<Sprite> dino_sprites;
vector<Sprite> ground_sprites;
Sprite reload_sprite;


float random(float min, float max){
	random_device generator;
	std::uniform_real_distribution<float> distrib(min, max);
	return distrib(generator);
}

Color colorWheel(int i, int n){
	return Color(128+127*sin(i*2*M_PI/n), 128+127*sin(i*2*M_PI/n + M_PI/3), 128+127*sin(i*2*M_PI/n + 2*M_PI/3));
}

class Obstacle{
public:
	int x;
	int h;
	int y;
	int L = 17;
	int nsprite = 0;
	
	bool pointInside(float xp, float yp){
		return ( ( x <= xp && xp <= x+L) && (y <= yp && yp <= y+h) );
	}
};

class Cactus : public Obstacle{
public:

	Cactus(float h, float x){
		this->h = h;
		this->x = x;
		this->y = 0;
		this->nsprite = round(random(0, cactus_sprites.size()-1));
	}
	
	void draw(int screen_height = 0){
		cactus_sprites[nsprite].drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
	}
};

class Oiseau : public Obstacle{
public:
	Oiseau(float h, float x, float y){
		this->h = h;
		this->x = x;
		this->y = y;
	}
};

class Dino{
public:
	int y;
	int x;
	int L = 42;
	int jambe_gauche = 10;
	int wbas = 17;
	int h = 46;
	int nsprite = 0;
	bool saut = false;
	float t_saut = 0;
	bool mort = false;
	int score = 0;
	chrono::system_clock::time_point t0;
	
	float a; // Coeff distance
	float b; // Coeff hauteur
	float c; // Coeff seuil
	
	float launch_saut = 0;
	
	float hsaut(float t){
		float h = 80;
		float l = 5*17;
		return -4*h*t*t/(l*l) + 4*h*t/l;
	}
	
	Dino(int x = 0, float a_anc = 1, float b_anc = 0.5, float c_anc = 0){
		this->x = x;
		this->a = a_anc;
		this->b = b_anc;
		this->c = c_anc;
		this->mort = false;
		t0 = chrono::system_clock::now();
		cout << "New dino : " << a_anc << ", " << b_anc << ", " << c_anc << endl;
	}
	
	void draw(int screen_height = 0){
		dino_sprites[nsprite].drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
	}
	
	void AI(int p_obst, int h_obst){
		
		launch_saut = a*float(p_obst-x)/(2*L) + b*float(h_obst)/float(h);
		if(launch_saut < c)
			saut = true;
	}
	
	void act(float& speed){
		
		if(saut){
			t_saut+=speed;
			y = int(hsaut(t_saut));
			nsprite = 2;
		} else {
			chrono::duration<float, std::milli> dt = chrono::system_clock::now() - t0;
			if(dt.count() > 100){
				nsprite = (nsprite + 1)%2;
				t0 = chrono::system_clock::now();
			}
		}
		
		if(y <= 0){
			saut = false;
			t_saut = 0;
			y = 0;
		}
		
		if(mort){
			x-=2*speed;
		}
		
	}
	
};

class World{
public:
	vector<Cactus> cactuses;
	vector<Dino> dinos;
	int longueur;
	int h_sol = 2;
	int L_sol = 15;
	int decalage_sol = 0;
	int morts = 0;
	int score = 0;
	int new_wave = 0;
	float speed = 1;
	
	chrono::system_clock::time_point t0;
	
	int generation = 0;
	vector<vector<float*> > entrees;
	vector<bool> dinos_morts;
	
	World(){
		
		for(unsigned i=0; i<N_DINOS; i++){
			dinos.push_back(Dino(5*16, random(-2, 2), random(-2, 2), random(-2, 2)));
		}
		morts = 0;
		
		t0 = chrono::system_clock::now();
	}
	
	void reset(){
		
		/******************* ALGORITHME GENETIQUE ***********************/
		
		
		/* On sélectionne les 2 meilleurs Dinos et on les "accouple" */
		vector<Dino> meilleurs;
		int max = 0;
		unsigned i_max = 0;
		for(unsigned i=0; i<dinos.size(); i++){
			if(dinos[i].score > max && !dinos[i].mort){
				max = dinos[i].score;
				i_max = i;
				cout << max << endl;
			}
		}
		meilleurs.push_back(dinos[i_max]);
		
		for(unsigned i=0; i<dinos.size(); i++){
			if(i == i_max)
				continue;
			if(dinos[i].score > max && !dinos[i].mort){
				max = dinos[i].score;
				i_max = i;
			}
		}
		meilleurs.push_back(dinos[i_max]);
		
		// On récupère les poids d'entréé des deux meilleurs Dinos
		
		float a1 = meilleurs[0].a;
		float b1 = meilleurs[0].b;
		float a2 = meilleurs[1].a;
		float b2 = meilleurs[1].b;
		
		float c1 = meilleurs[0].c;
		float c2 = meilleurs[1].c;
		
		dinos.clear();
		
		// On réinitialise les Dinos et on créee les nouveaux à partir des 2 meilleurs précédemment choisis
		const float p_cross_over = 0.5;
		const float p_mutation_importante = 10e-2;
		
		for(unsigned i=0; i<N_DINOS; i++){
			float r = random(0, 1);
			float r2 = random(0, 1);
			float a, b, c;
			
			// Cross Overs
			if(r > p_cross_over)
				a = a1;
			else
				a = a2;
			if(r2 > p_cross_over)
				b = b1;
			else
				b = b2;
			if(random(0, 1) > p_cross_over)
				c = c1;
			else
				c = c2;
			
			
			// Mutations
			a = a + random(-0.04, 0.04);
			b = b + random(-0.04, 0.04);
			c = c + random(-0.04, 0.04);
			
			if(random(0, 1) < p_mutation_importante){
				a = random(-2, 2);
			}
			if(random(0, 1) < p_mutation_importante){
				b = random(-2, 2);
			}
			if(random(0, 1) < p_mutation_importante){
				c = random(-2, 2);
			}
			
			dinos.push_back(Dino(5*16, a, b, c));
		}

		// Reset du monde
		t0 = chrono::system_clock::now();
		morts = 0;
		score = 0;
		new_wave = 0;
		speed = 1;
		generation++;
		cactuses.clear();
	}
	
	/*void saveHighScore(){
		fstream fs;
		fs.open("highscore.txt", fs.out | fs.in);
		string contenu;
		std::getline(fs, contenu);
		if(contenu.length() > 0){
			int prec_high_score = std::stoi(contenu);
			if(score > prec_high_score){
				std::remove("highscore.txt");
				fs << score;
				fs.close();
			}
		} else {
			fs << score;
			fs.close();
		}
		
	}*/
	
	// Génère des cactus selon des patterns prédéfinis.
	void spawnCactus(){
		int r = round(random(0, 3));
		//cout << "R = " << r << endl;
		switch(r){
			case 0:
				cactuses.push_back(Cactus(20, longueur));
				cactuses.push_back(Cactus(30, longueur+32));
				cactuses.push_back(Cactus(20, longueur+32*2));
				break;
			case 1:
				cactuses.push_back(Cactus(50, longueur));
				break;
			case 2:
				cactuses.push_back(Cactus(50, longueur));
				cactuses.push_back(Cactus(20, longueur+32));
				break;
			case 3:
				cactuses.push_back(Cactus(35, longueur));
				cactuses.push_back(Cactus(35, longueur+32));
				break;
			default:
				cerr << "Bug de generation !" << endl;
		}
	}
	
	bool dinoMeurt(Dino dino){
		for(Cactus &c: cactuses){
			if(c.pointInside(dino.x+dino.jambe_gauche+dino.wbas, dino.y) || c.pointInside(dino.x+dino.jambe_gauche, dino.y) || c.pointInside(dino.x+dino.L, dino.y+dino.h))
				return true;
		}
		return false;
	}
	
	void iteration(){
		score+=speed;
		if(new_wave <= 0){
			spawnCactus();
			new_wave = 200+int(random(-20, 20));
		}
		new_wave -= speed;
		
		for(unsigned c=0; c < cactuses.size(); c++){
			
			cactuses[c].x-=2*speed;
			
			if((cactuses[c].x + cactuses[c].L) < 0){
				cactuses.erase(cactuses.begin()+c);
			}
		}
		
		decalage_sol = (decalage_sol - 2*speed);
		
		
		int dino_max = 0;
		int dino = 0;
		for(unsigned d; d<dinos.size(); d++){
			if(dinos[d].score > dino_max && !dinos[d].mort){
				dino_max = dinos[d].score;
				dino = d;
			}
		}
		
		int cactus_min = 1000;
		int c_min = 0;
		for(unsigned c=0; c<cactuses.size(); c++){
			if(cactuses[c].x >= dinos[dino].x && cactus_min > cactuses[c].x){
				cactus_min = cactuses[c].x;
				c_min = c;
			}
		}
		
		for(unsigned d=0; d<dinos.size(); d++){
			
			dinos[d].AI(cactus_min, cactuses[c_min].h);
			// Sortie de l'IA
			
			
			//cout << "Saut (" << d << ") : " << dinos[d].launch_saut << ", (" << dinos[d].a << ";" << dinos[d].b << ")" << endl;
			
			dinos[d].act(speed);
			if(!dinos[d].mort){
				if(dinoMeurt(dinos[d])){
					dinos[d].mort = true;
					dinos[d].score = score;
					morts++;
					//cout << "Dino n°" << d << " est MORT!" << endl;
				}
			}
		}
		
		speed = floor(score / 1500) + 1;
	}
};

void redraw(Allegro* allegro, float FPS){
	allegro->draw_rectangle(0, 0, allegro->getDisplayWidth(), allegro->getDisplayHeight(), allegro->white, 1, true);
	
	World* world = (World*)(allegro->getContext());
	vector<Dino>* dinos = &(world->dinos);
	int largeur = allegro->getDisplayWidth();
	int hauteur = allegro->getDisplayHeight();
	
	
	for(int i = 0; i<(largeur/world->L_sol + 2); i++){
		ground_sprites[(i-(world->decalage_sol/world->L_sol))%ground_sprites.size()].drawSprite(i*world->L_sol+(world->decalage_sol)%world->L_sol, hauteur-(67-54));
	}
	
	for(Cactus &c: world->cactuses){
		c.draw(hauteur-world->h_sol);
	}
	
	for(unsigned dino=0; dino<dinos->size(); dino++){
		stringstream num;
		num << dino;
		allegro->draw_text(dinos->at(dino).x+20, hauteur-dinos->at(dino).y-60, num.str(), colorWheel(dino, dinos->size()).toAllegro(), ALLEGRO_ALIGN_CENTRE);
		
		dinos->at(dino).draw(hauteur-world->h_sol);
		
	}

//	if(world->mort){
//		allegro->draw_text(largeur/2, hauteur/2, "You dead bro.", Color(255, 0, 0).toAllegro(), ALLEGRO_ALIGN_CENTRE);
//		
//		reload_sprite.drawSprite(largeur/2-17, hauteur/2+20);
//	}
	
	stringstream score_ss;
	score_ss << "Score : " << world->score;
	allegro->draw_text(0, 0, score_ss.str(), Color(0, 0, 0).toAllegro(), ALLEGRO_ALIGN_LEFT);
	
	stringstream level_ss;
	level_ss << "Level : " << world->speed;
	allegro->draw_text(0, 15, level_ss.str(), Color(0, 0, 0).toAllegro(), ALLEGRO_ALIGN_LEFT);
	
	stringstream gen_ss;
	gen_ss << "Génération : " << world->generation;
	allegro->draw_text(0, 30, gen_ss.str(), Color(0, 0, 0).toAllegro(), ALLEGRO_ALIGN_LEFT);

}

void animate(Allegro* allegro, float FPS){
	World* world = (World*)(allegro->getContext());
	if(world->morts < world->dinos.size())
		world->iteration();
	else
		world->reset();
}

void onKeyDown(Allegro* allegro, void* context, uint16_t event, uint8_t keycode){
	/*World* world = (World*)context;
	vector<Dino>* dinos = &(world->dinos);
	Dino* dino = &(dinos->at(0));
	if(keycode == ALLEGRO_KEY_SPACE && !dino->saut && dino->y == 0){
		dino->saut = true;
	}*/
//	if(world->mort && keycode == ALLEGRO_KEY_SPACE){
//		world->reset();
//	}
}

int main(int argc, char **argv)
{
	Allegro allegro;
	World world;
	world.longueur = 500;
	allegro.init();
	allegro.createWindow(60, world.longueur, 300);
	allegro.setContext(&world);
	
	spritemap = SpriteMap("assets/spritemap2.png");
	
	for(unsigned i=0; i<6; i++){
		cactus_sprites.push_back(spritemap.getSprite(228+i*(245-228), 0, 245-228, 36));
	}
	for(unsigned i=2; i<=3; i++){
		dino_sprites.push_back(spritemap.getSprite(678+i*(720-677+1), 2, 720-677, 48-2));
	}
	dino_sprites.push_back(spritemap.getSprite(678, 2, 720-677, 48-2)); // jump dino
	
	for(unsigned i=0; i<74; i++){
		ground_sprites.push_back(spritemap.getSprite(2+i*(17-2+1), 54, 17-2, 67-54));
	}
	
	reload_sprite = spritemap.getSprite(2, 2, 37-2, 33-2);
	
	allegro.setRedrawFunction(&redraw);
	allegro.setAnimateFunction(&animate);
	allegro.bindKeyDown(&onKeyDown);
	
	allegro.gameLoop();
}
