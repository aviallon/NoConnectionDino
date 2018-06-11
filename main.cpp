#include <iostream>
#include <random>
#include <sstream>
#include <fstream>
#include <cmath>
#include <chrono>
#include <string>
#include <cstdio>
#include <utility>
#include <thread>
#include "allegro/allegro.h"

#define N_DINOS 70
#define CDEF (random(-3, 3))

using namespace std;

SpriteMap spritemap;
vector<Sprite> cactus_sprites;
vector<Sprite> bird_sprites;
vector<Sprite> dino_sprites_run;
vector<Sprite> dino_sprites_bent;
Sprite dino_sprite_dead;
Sprite dino_sprite_jump;
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
    vector<Sprite>* sprites;
	
	bool pointInside(float xp, float yp){
		return ( ( x <= xp && xp <= x+L) && (y <= yp && yp <= y+h) );
	}
    
    void draw(int screen_height = 0){
        sprites->at(nsprite).drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
    }
};

class Cactus : public Obstacle{
public:

	Cactus(float h, float x){
		this->h = h;
		this->x = x;
		this->y = 0;
        this->sprites = &cactus_sprites;
		this->nsprite = round(random(0, cactus_sprites.size()-1));
	}
};

class Oiseau : public Obstacle{
public:
	Oiseau(float x, float y){
		this->h = 33;
        this->L = 42;
		this->x = x;
		this->y = y;
        this->sprites = &bird_sprites;
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
    bool bent = false;
	float t_saut = 0;
	bool mort = false;
    bool alwaysjump = true;
	int score = 0;
	chrono::system_clock::time_point t0;
	
	vector<float> coeffs; // Coefficients utilisés pour l'IA\
	0 -> coeff position cactus\
	1 -> coeff hauteur cactus\
	2 -> coeff seuil
	
	float launch_saut = 0;
	
	float hsaut(float t){
		float h = 80;
		float l = 5*17;
		return -4*h*t*t/(l*l) + 4*h*t/l;
	}
	
	Dino(int x, vector<float> coeffs){
		this->x = x;
		this->coeffs = coeffs;
		this->mort = false;
		t0 = chrono::system_clock::now();
		
	}
	
	void draw(int screen_height = 0){
        if(mort)
            dino_sprite_dead.drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
        else if(saut)
            dino_sprite_jump.drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
        else if(bent)
            dino_sprites_bent[nsprite].drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
		else
            dino_sprites_run[nsprite].drawSprite(this->x, screen_height-(this->y+this->h), this->L, this->h);
	}
	
	
	/**
	 * @brief The Neural-network is used to decide which action to do based on the given ouptuts
	 * @param p_obst
	 * @param h_obst
	 */
	void AI(int p_obst, int h_obst){
		
		launch_saut = coeffs[0]*float(p_obst)/float(L) + coeffs[1]*float(h_obst)/float(h);
		if(launch_saut > coeffs[2]){
			saut = true;
        } else {
            if(!saut)
                alwaysjump = false;
        }
		
	}
	
	void act(float& speed){
		
		if(saut){
			t_saut+=speed;
			y = int(hsaut(t_saut));
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
	
    static bool compare(Dino& a, Dino &b){
        return (a.score < b.score);
    }
    
    static bool compare_desc(Dino& a, Dino &b){
        return (a.score > b.score);
    }
    
    static bool compare_desc_alive(Dino& a, Dino &b){
        if(a.mort)
            return false;
        if(b.mort)
            return true;
        return (a.score > b.score);
    }
    
    /**
     * @brief Discriminates Dinos based on their best score and class as lowest Dinos which always jump
     * @param a First Dino
     * @param b Second Dino
     * @return Returns true if the first Dino is better than the second one
     */
    static bool compare_desc_noalwaysjump(Dino& a, Dino &b){
        if(a.alwaysjump)
            return false;
        if(b.alwaysjump)
            return true;
        return (a.score > b.score);
    }
	
	static bool is_dead(Dino& d){
		return d.mort;
	}
};

class World{
public:
	vector<Obstacle> obstacles;
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
    
    vector<int> generation_high_scores;
	
	World(){
		
		for(unsigned i=0; i<N_DINOS; i++){
			vector<float> coeffs;
			for(unsigned j = 0; j<3; j++){
				coeffs.push_back(CDEF);
			}
			dinos.push_back(Dino(5*16, coeffs));
		}
		
		morts = 0;
		
		t0 = chrono::system_clock::now();
	}
	
	void reset(){
        
		/******************* ALGORITHME GENETIQUE ***********************/
		
		
		/* On sélectionne les 2 meilleurs Dinos et on les "accouple" */
        sort(dinos.begin(), dinos.end(), Dino::compare_desc_noalwaysjump);
        
        generation_high_scores.push_back(dinos[0].score);
		
		// On récupère les poids d'entréé des deux meilleurs Dinos
		
		vector<vector<float> > coeffs_dinos(2);
		
		coeffs_dinos[0] = dinos[0].coeffs;
		coeffs_dinos[1] = dinos[1].coeffs;
		
		dinos.clear();
        dinos = vector<Dino>(N_DINOS, Dino(0, vector<float>(3, 0)));
		
		// On réinitialise les Dinos et on créee les nouveaux à partir des 2 meilleurs précédemment choisis
		const float p_cross_over = 0.2;
		const float p_mutation_importante = 10e-2;
		const float taux_mutation = 0.08;
		
       // #pragma omp for ordered schedule(dynamic)
        #pragma omp parallel for
		for(unsigned i=0; i<N_DINOS; i++){
			
			vector<float> this_dino_coeffs = coeffs_dinos[0];
			for(unsigned c=0; c<coeffs_dinos[0].size(); c++){
				if(random(0, 1) > 1-p_cross_over)
					this_dino_coeffs[c] = coeffs_dinos[1][c];
					
				this_dino_coeffs[c] += random(-taux_mutation/2, taux_mutation/2);
				
				if(random(0, 1) > p_mutation_importante)
					this_dino_coeffs[c] = CDEF;
			}
			
            //stringstream ss;
            //ss << "Dino (" << i << ") : " << a << ", " << b << ", " << c << endl;
            //cout << ss.str();
            
           // #pragma omp ordered
            dinos[i] = Dino(5*16, this_dino_coeffs);
		}

		// Reset du monde
		t0 = chrono::system_clock::now();
		morts = 0;
		score = 0;
		new_wave = 0;
		speed = 1;
		generation++;
		obstacles.clear();
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
	void spawnObstacles(){
		int r = round(random(0, 3));
		//cout << "R = " << r << endl;
		switch(r){
			case 0:
				obstacles.push_back(Cactus(20, longueur));
				obstacles.push_back(Cactus(30, longueur+32));
				obstacles.push_back(Cactus(20, longueur+32*2));
				break;
			case 1:
				obstacles.push_back(Cactus(50, longueur));
				break;
			case 2:
				obstacles.push_back(Cactus(50, longueur));
				obstacles.push_back(Cactus(20, longueur+32));
				break;
			case 3:
				obstacles.push_back(Cactus(35, longueur));
				obstacles.push_back(Cactus(35, longueur+32));
				break;
            case 4:
                obstacles.push_back(Oiseau(longueur, 35));
                break;
			default:
				cerr << "Bug de generation !" << endl;
		}
	}
	
	bool dinoMeurt(Dino dino){
		for(Obstacle &c: obstacles){
			if(c.pointInside(dino.x+dino.jambe_gauche+dino.wbas, dino.y) || c.pointInside(dino.x+dino.jambe_gauche, dino.y) || c.pointInside(dino.x+dino.L, dino.y+dino.h))
				return true;
		}
		return false;
	}
	
	void iteration(){
		//morts = std::count_if(dinos.begin(), dinos.end(), Dino::is_dead);
		
		score += speed;
		if(new_wave <= 0){
			spawnObstacles();
			new_wave = 200+int(random(-80, 10));
		}
		new_wave -= speed;
		
		for(unsigned c=0; c < obstacles.size(); c++){
			
			obstacles[c].x-=2*speed;
			
			if((obstacles[c].x + obstacles[c].L) < 0){
				obstacles.erase(obstacles.begin()+c);
			}
		}
		
		decalage_sol = (decalage_sol - 2*speed);
		
		
        /* Gets nearest cactus */
        
        vector<Dino> _dinos = dinos;
        sort(_dinos.begin(), _dinos.end(), Dino::compare_desc_alive);
		
		int obstacle_min = 1000;
		int c_min = 0;
		for(unsigned c=0; c<obstacles.size(); c++){
			if(obstacles[c].x >= _dinos[0].x && obstacle_min > obstacles[c].x){
				obstacle_min = obstacles[c].x;
				c_min = c;
			}
		}
		
        //#pragma omp for ordered schedule(dynamic)
		for(unsigned d=0; d<dinos.size(); d++){
			
            if(!dinos[d].mort){
                
                dinos[d].AI(obstacle_min, obstacles[c_min].h);
                // Sortie de l'IA
                
                
				if(dinoMeurt(dinos[d])){
					dinos[d].mort = true;
					dinos[d].score = score;
                    
                    //#pragma omp ordered
					morts++;
                    //cout << "\t" << "Morts : " << morts << endl;
				}
			}
			
            dinos[d].act(speed);
		}
		
		speed = floor(score / 1500) + 1;
		
		if(morts >= dinos.size()){
			reset();
		}
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
	
	for(Obstacle &c: world->obstacles){
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
	world->iteration();
}

chrono::system_clock::time_point temps_appui_reset;
bool disableReset = false;

void onKey(Allegro* allegro, void* context, uint16_t event, uint8_t keycode){
	World* world = (World*)context;
    
    if(event & Allegro::KEY_DOWN){
        if(keycode == ALLEGRO_KEY_R){
            temps_appui_reset = chrono::system_clock::now();
        }
    }
	if(event & Allegro::KEY_UP){
        if(keycode == ALLEGRO_KEY_R){
            disableReset = false;
        }
    }
    
    chrono::duration<float, std::milli> dt = chrono::system_clock::now() - temps_appui_reset;
    if(allegro->isKeyDown(ALLEGRO_KEY_R) && dt.count() > 200 && !disableReset){
        world->reset();
		allegro->setRedrawFunction(&Allegro::_undefined_);
		this_thread::sleep_for(chrono::milliseconds(200));
		allegro->setRedrawFunction(&redraw);
        disableReset = true;
    }
	/*vector<Dino>* dinos = &(world->dinos);
	Dino* dino = &(dinos->at(0));
	if(keycode == ALLEGRO_KEY_SPACE && !dino->saut && dino->y == 0){
		dino->saut = true;
	}*/
//	if(world->mort && keycode == ALLEGRO_KEY_SPACE){
//		world->reset();
//	}
}

pair<int, int> pointToPixel(Allegro* allegro, float xmax, float ymax, pair<float, float> point, pair<int, int> topLeft, pair<int, int> bottomRight){
    
    int largeur = bottomRight.first - topLeft.first;
    int hauteur = bottomRight.second - topLeft.second;
    
    pair<int, int> res;
    res.first = point.first*largeur/xmax + topLeft.first;
    res.second = hauteur-point.second*hauteur/ymax + topLeft.second;
    
    return res;
}

void grapheRedraw(Allegro* allegro, float fps){
    allegro->clearScreen();
    
    World* world = (World*)(allegro->getContext());
    vector<int>* hscores = &(world->generation_high_scores);
    
    int largeur = allegro->getDisplayWidth();
    int hauteur = allegro->getDisplayHeight();
    
    //float percent = max(float(world->score)/40000, 0.0f);
    
    pair<int, int> topLeft(10, 10);
    pair<int, int> bottomRight(largeur-10, hauteur-10);
    allegro->draw_rectangle(topLeft.first, topLeft.second, bottomRight.first, bottomRight.second, Color(0, 0, 0).toAllegro(), 1, false);
    
    vector<int>::iterator max_score = max_element(hscores->begin(), hscores->end());
    float xmax = max(10, int(hscores->size()));
    float ymax = 5000;
    pair<int, int> prev_pix;
    if(hscores->size() > 0){
        ymax = max(ymax, float(hscores->at(distance(hscores->begin(), max_score))));
    }
    ymax = max(ymax, float(world->score));
    
    for(unsigned i=0; i<hscores->size(); i++){
        //cout << hscores->at(i) << endl;
        pair<int, int> pix = pointToPixel(allegro, xmax, ymax, pair<float, float>(i, hscores->at(i)), topLeft, bottomRight);
        Color couleur = colorWheel(i, hscores->size());
        
        allegro->draw_rectangle(pix.first-2, pix.second-2, pix.first+2, pix.second+2, couleur.toAllegro(), 1, true);
        
        if(i!=0)
            allegro->draw_line(prev_pix.first, prev_pix.second, pix.first, pix.second);
    
        stringstream ss;
        ss << i;
        allegro->draw_text(pix.first, pix.second-15, ss.str(), couleur.toAllegro(), ALLEGRO_ALIGN_CENTRE);
        
        prev_pix = pix;
    }
    
    pair<int, int> pix = pointToPixel(allegro, xmax, ymax, pair<float, float>(world->generation, world->score), topLeft, bottomRight);
    Color couleur = colorWheel(world->generation, hscores->size());
	
	if(hscores->size() > 0){
		allegro->draw_line(prev_pix.first, prev_pix.second, pix.first, pix.second, Color(150, 150, 150).toAllegro(), 1);
	}
    
    allegro->draw_ellipse(pix.first-3, pix.second-3, pix.first+3, pix.second+3, couleur.toAllegro(), 1, false);
    //allegro->draw_rectangle(pix.first-2, pix.second-2, pix.first+2, pix.second+2, couleur.toAllegro(), 1, true);
        
    
    stringstream maxscore;
    maxscore << ymax;
    allegro->draw_text(topLeft.first, topLeft.second, maxscore.str(), Color(0, 0, 255).toAllegro(), ALLEGRO_ALIGN_LEFT);
    
    //allegro->draw_rectangle(20, largeur/2+20, (longueur-40)+20, largeur/2-20, Color(0, 0, 0).toAllegro(), 2, false);
    //allegro->draw_rectangle(20, largeur/2+20, round((longueur-40)*percent)-20, largeur/2+20, Color(0, 0, 0).toAllegro(), 1, true);
}

int main(int argc, char **argv)
{
    Allegro::init();
	Allegro jeu, graphe;
	World world;
	world.longueur = 500;
	jeu.createWindow(120, world.longueur, 300);
	jeu.setContext(&world);
    
    graphe.createWindow(15, 400, 300);
    graphe.setContext(&world);
    
    graphe.setRedrawFunction(&grapheRedraw);
	
	spritemap = SpriteMap("assets/spritemap2.png");
	
	for(unsigned i=0; i<6; i++){
		cactus_sprites.push_back(spritemap.getSprite(228+i*(245-228), 0, 245-228, 36));
	}
    for(unsigned i=0; i<1; i++){
		bird_sprites.push_back(spritemap.getSprite(134+i*(179-134), 2, 179-134, 41-2));
	}
	for(unsigned i=2; i<=3; i++){
		dino_sprites_run.push_back(spritemap.getSprite(678+i*(720-677+1), 2, 720-677, 48-2));
	}
	dino_sprite_jump = spritemap.getSprite(678, 2, 720-677, 48-2); // jump dino
    
    dino_sprite_dead = spritemap.getSprite(853, 2, 720-677, 48-2); // dead dino
	
	for(unsigned i=0; i<74; i++){
		ground_sprites.push_back(spritemap.getSprite(2+i*(17-2+1), 54, 17-2, 67-54));
	}
	
	reload_sprite = spritemap.getSprite(2, 2, 37-2, 33-2);
	
	jeu.setRedrawFunction(&redraw);
	jeu.setAnimateFunction(&animate);
	jeu.bindKeyDown(&onKey);
	jeu.bindKeyUp(&onKey);
	
	Allegro::startLoop();
}
