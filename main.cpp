#include <time.h>
#include <stdio.h>
#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#define CarColor 10
#define BoardDim 20
#define RoadHeight 1
#define RoadNumber 8
#define ObstacleNumber 9
#define JumpDelay 0.2
#define FrameDelay 30000
#define RefreshDelay 15000
#define TextHeight 10
#define FriendlyPushDistance 1
#define StoppingDistance 3

using namespace std;

typedef struct
{
    int playing_area_width;
    int playing_area_height;

    int number_of_friendly_cars;

    int number_of_hostile_cars;

    int number_of_stopping_cars;
} GameConfig;

typedef struct
{
    int x;
    int y;
} Frog;

typedef struct
{
    int x;
    int y;
} Finish;

typedef struct
{
    int x;
    int y;
    int direction;
    int speed;
    int is_static;
} Car;

typedef struct
{
    int x;
    int y;
} Obstacle;

typedef struct
{
    int waiting_to_push;
    int car_index;
} PushingCar;

//Load configuration from file
int load_config(const char *file, GameConfig *config)
{
    FILE *config_game = fopen(file, "r");
    if (config_game == nullptr)
    {
        perror("Cannot open config file");
        return 1;
    }

    fscanf(config_game, "playing_area_width %d\n", &config->playing_area_width);
    fscanf(config_game, "playing_area_height %d\n", &config->playing_area_height);
    fscanf(config_game, "number_of_friendly_cars %d\n", &config->number_of_friendly_cars);
    fscanf(config_game, "number_of_hostile_cars %d\n", &config->number_of_hostile_cars);
    fscanf(config_game, "number_of_stopping_cars %d\n", &config->number_of_stopping_cars);

    fclose(config_game);
    return 0;
}

//Generate random number between min and max
int get_random_number(int min, int max)
{
    return rand() % (max + 1 - min) + min;
}

//Initialize color pairs
void initialize_colors()
{
    start_color();
    //           Foreground   Background
    init_pair(1, COLOR_BLACK, COLOR_GREEN);   // Frog color
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);  // Street color
    init_pair(3, COLOR_BLACK, COLOR_RED);     // Hostile car color
    init_pair(4, COLOR_BLACK, COLOR_BLUE);    // Friendly car color
    init_pair(5, COLOR_BLACK, COLOR_MAGENTA); // Stopping car color
    init_pair(6, COLOR_BLACK, COLOR_WHITE);   // Obstacle color

    init_pair(7, COLOR_WHITE, COLOR_BLACK); // Text color
    init_pair(8, COLOR_RED, COLOR_BLACK);   // "FROGGER" color
}


//BOARD FUNCTIONS

//Create game board window
WINDOW *create_board(int BoardHeight, int BoardWidth)
{
    int xMax, yMax; // zmienne do przechowywania terminala
    getmaxyx(stdscr, yMax, xMax); // pobierania wysokosci i szerokosci terminala

    refresh();
    // tworzenie okna z wymiarami BoardHeight x BoardWidth, umieszczenia okna w srodku terminala, obliczenie wspolrzednych jego srodka
    WINDOW *board_win = newwin(BoardHeight, BoardWidth, (yMax / 2) - (BoardHeight / 2), (xMax / 2) - (BoardWidth / 2));

    if (!board_win) // czy udalo sie utworzyc okno
    {
        printw("Error: Could not create board window\n");
        refresh();
        return nullptr;
    }

    box(board_win, 0, 0); // rysowanie ramki board_win dla domyslnych znakow
    wrefresh(board_win); // odswiezanie okna by wyswietlic ramke
    return board_win;
}

//Draw board borders
void draw_board(WINDOW *board_win)
{
    box(board_win, 0, 0); // narysuj ramke dla boardwin
}

//Display game information
void display_info(WINDOW *board_win, int elapsed_time, GameConfig config)
{
    int x, y;
    getbegyx(board_win, y, x);
    int width = getmaxx(board_win);

    int start_x = x + width + 5;

    if (start_x + 20 > COLS)
    {
        mvprintw(TextHeight, start_x, "Info Area Overflow");
        return;
    }

    attron(COLOR_PAIR(7));
    mvprintw(TextHeight, start_x, "Name: %s", "Karol");
    mvprintw(TextHeight + 1, start_x, "Surname: %s", "Obrycki");
    mvprintw(TextHeight + 2, start_x, "Index: %d", 203264);
    mvprintw(TextHeight + 3, start_x, "Time: %d seconds", elapsed_time);
    attroff(COLOR_PAIR(7));
}

//Display "FROGGER" text
void frogger_text(WINDOW *board_win)
{//wypisanie napisu FROGGER na samym srodku na gorze
    int xMax, yMax;
    getbegyx(board_win, yMax, xMax);
    int width = getmaxx(board_win);

    attron(COLOR_PAIR(8));
    mvprintw(yMax - 2, xMax + (width - strlen("FROGGER")) / 2 + 1, "FROGGER");
    attroff(COLOR_PAIR(8));
}


//ENDING FUNCTIONS

//Initialize finish area
void creare_finish(WINDOW *board_win, Finish *finish, GameConfig config)
{   //wspolrzedne dla konca gry
    finish->x = config.playing_area_width / 2;
    finish->y = 1;

    //wyrysuj koniec gry
    wattron(board_win, COLOR_PAIR(1));
    mvwprintw(board_win, finish->y, finish->x, " ");
    wattroff(board_win, COLOR_PAIR(1));
}

//Check if frog has reached the finish
bool check_finish_collision(Frog *frog, Finish *finish)
{
    return (frog->x == finish->x && frog->y == finish->y);
}

void display_win_message(WINDOW *board_win, int elapsed_time, GameConfig config)
{
    int rows, cols;
    getmaxyx(board_win, rows, cols);

    const char *message = "You Won!";
    int x = (cols - strlen(message)) / 2;
    int y = rows / 2;

    wattron(board_win, A_BOLD);
    mvwprintw(board_win, y, x, "%s", message);
    mvwprintw(board_win, y + 1, x - 5, "You've got %d points", config.playing_area_width / elapsed_time * 3);
    wattroff(board_win, A_BOLD);
    wrefresh(board_win);
    usleep(2000000);
}

void display_lose_message(WINDOW *board_win)
{
    int rows, cols;
    getmaxyx(board_win, rows, cols);

    const char *message = "You Lose!";
    int x = (cols - strlen(message)) / 2 + 1;
    int y = rows / 2;

    wattron(board_win, A_BOLD);
    mvwprintw(board_win, y, x, "%s", message);
    wattroff(board_win, A_BOLD);
    wrefresh(board_win);
    usleep(2000000);
}



//ROAD FUNCTIONS

//Initialize road flags
void initialize_flags(int *flags, int size)
{
    for (int i = 0; i < size; i++)
    {
        flags[i] = 0;
    }
}

//Check roads collision
bool is_roads_collision(int *flags, int road_y, int road_height, int rows)
{
    if (road_y < 0 || road_y >= rows) // sprawdzanie czy droga jest w obszarze planszy
    {
        return true;
    }
    return flags[road_y] != 0; // sprawdzanie czy droga jest juz zajeta (!= 0 == true, my chcemy false)
}

//Mark a road for checking the collision
void mark_road(int *flags, int road_y, int road_height, int rows)
{
    if (road_y >= 0 && road_y < rows) // czy droga w obszarze planszy
    {
        flags[road_y] = 1; // oznaczamy nowa droge
    }
}

void draw_one_road(WINDOW *board_win, int road_y, int cols)
{
    wattron(board_win, COLOR_PAIR(2)); // ustawienie koloru dla drogi w board_win
    for (int x = 0; x < cols - 2; x++) // rysowanie drogi
    {
        mvwprintw(board_win, road_y, x + 1, " ");
    }
    wattroff(board_win, COLOR_PAIR(2)); // wylaczenie koloru drogi dla board_win
}

//Draw all roads
void draw_roads(int *used_flags, WINDOW *board_win, int rows, int cols)
{   //rysowanie drogi dla wspolrzednych y z tablicy
    for (int y = 0; y < rows; y++)
    {
        if (used_flags[y] == 1)
        {
            draw_one_road(board_win, y, cols);
        }
    }
}

void get_random_road(int *used_flags, int rows)
{
    srand(time(NULL));

    int road_count = 0;

    while (road_count < RoadNumber)
    {
        int road_y = rand() % (rows - RoadHeight - 4) + 2; // Generowanie dróg od 2 do rows - 2

        //Check if there is already a road in this position
        if (is_roads_collision(used_flags, road_y, RoadHeight, rows))
        {
            continue;
        }

        mark_road(used_flags, road_y, RoadHeight, rows);
        road_count++;
    }
}


//OBSTACLE FUNCTIONS

void initialize_obstacle(Obstacle *obstacles, const int *used_flags, int rows, int cols)
{
    int obstacle_i = 0; //indeks przeszkody z tablicy obstacles[]
    int obstacle_count = 0; //liczymy przeszkody

    bool check_k[rows] = {false}; //tablica boolow ktore wiesze dla przeszkod sa juz zajete

    while (obstacle_count < ObstacleNumber)
    {
        int k = rand() % rows; // losowanie y dla przeszkody
        if (used_flags[k] != 0 || check_k[k] == true) // sprawdzamy czy droga wynosi 1 lub juz jest na niej przeszkoda
        {
            continue;
        }
        if (k > rows - 5 || k < 3) //drugie sprawczenie czy y dla preszkody miesci sie w zakresie planszy
        {
            continue;
        }
        obstacles[obstacle_i].y = k; //y dla przeszkody
        obstacles[obstacle_i].x = rand() % (cols - 6) + 1; // losowanie x dla przeszkody
        obstacle_i++; // przechodzimy do indeksu nastepnej przeszkody
        check_k[k] = true; //dajemy jej y na true
        obstacle_count++; // liczymy przeszkody
    }
}

void draw_obstacles(WINDOW *board_win, Obstacle *obstacles, int obstacle_count)
{
    for (int i = 0; i < obstacle_count; i++)
    {
        wattron(board_win, COLOR_PAIR(6)); // wlaczamy kolor dla board_win
        mvwprintw(board_win, obstacles[i].y, obstacles[i].x, "#"); // rysowanie danej przeszkody
        wattroff(board_win, COLOR_PAIR(6)); // wlaczamy kolor dla board_win
    }
}

//Check collision with the frog and obstacles
bool check_collision_obstacle(Frog *frog, Obstacle *obstacles, int obstacle_count)
{ // sprawdzenie czy zaba nie bedzie wchodzi na przeszkode
    for (int i = 0; i < obstacle_count; i++)
    {
        if ((frog->x == obstacles[i].x && frog->y == obstacles[i].y) ||
            (frog->x == obstacles[i].x && frog->y + 1 == obstacles[i].y))
        {
            return true;
        }
    }
    return false;
}


//CARS FUNCTIONS

void initialize_cars(Car *cars, const int *used_flags, int rows, int cols)
{
    int car_i = 0; // liczby samochody

    for (int i = 0; i < rows; i++)
    {
        if (used_flags[i] != 1) // jesli droga nie jest zainicjalizowana kontynnuj
        {
            continue;
        }
        cars[car_i].y = i; // wiersz y dla samochodu
        cars[car_i].x = get_random_number(4, cols - 6); // losowanie x dla samochodu pomiedzy 4 a cols - 6
        cars[car_i].direction = (rand() % 2 == 0) ? 1 : -1; // losujemy kierunek dla samochodu (1 - prawo, -1 - lewo)
        cars[car_i].speed = get_random_number(1, 3); // losowanie predkosci samochodu
        car_i++;
        if (car_i >= RoadNumber) // sprawdzanie czy juz zainicjalizowano wszystkie samochody
        {
            break;
        }
    }
}

//Initialize car colors and positions
void initialize_car_colors(GameConfig config, int *hostile_positions, int *friendly_positions, int *stopping_positions)
{
    static bool initialized = false; //kod wykona sie raz

    if (!initialized)
    {
        bool if_placed[RoadNumber] = {false}; // tablica czy droga juz jest na danym y

        int car_count1 = 0, car_count2 = 0, car_count3 = 0; // zliczanie drog dla poszczegolnych kolorow samochodow

        while (car_count1 < config.number_of_hostile_cars) // wrogie samochody
        {
            int k = rand() % RoadNumber;
            if (!if_placed[k]) // losujemy caly czas drogi az znajdziemy taka na ktorej jeszcze nie ma samochodu
            {
                if_placed[k] = true; // oznaczamy droge jako zajeta
                hostile_positions[car_count1] = k; // oznaczamy y dla wrogiego samochodu
                car_count1++; // zwiekszamy jego liczbe
            }
        }

        while (car_count2 < config.number_of_friendly_cars)
        {
            int k = rand() % RoadNumber;
            if (!if_placed[k])
            {
                if_placed[k] = true;
                friendly_positions[car_count2] = k;
                car_count2++;
            }
        }

        while (car_count3 < config.number_of_stopping_cars)
        {
            int k = rand() % RoadNumber;
            if (!if_placed[k])
            {
                if_placed[k] = true;
                stopping_positions[car_count3] = k;
                car_count3++;
            }
        }
        initialized = true; // dajemy ze juz wykonalismy ta funcje
    }
}

//Draw all cars
void draw_cars(WINDOW *board_win, Car *cars, GameConfig config, int *hostile_positions, int *friendly_positions, int *stopping_positions)
{
    initialize_car_colors(config, hostile_positions, friendly_positions, stopping_positions);

    //Draw hostile cars (Red)
    for (int i = 0; i < config.number_of_hostile_cars; i++)
    {
        wattron(board_win, COLOR_PAIR(3)); // wlaczamy kolor dla board_win
        mvwprintw(board_win, cars[hostile_positions[i]].y, cars[hostile_positions[i]].x, "o-o"); // rysujemy wrogi samochod na danej pozycji y i x
        wattroff(board_win, COLOR_PAIR(3));
    }

    //Draw friendly cars (Blue)
    for (int i = 0; i < config.number_of_friendly_cars; i++)
    {
        wattron(board_win, COLOR_PAIR(4));
        mvwprintw(board_win, cars[friendly_positions[i]].y, cars[friendly_positions[i]].x, "o-o");
        wattroff(board_win, COLOR_PAIR(4));
    }

    //Draw stopping cars (Magenta)
    for (int i = 0; i < config.number_of_stopping_cars; i++)
    {
        wattron(board_win, COLOR_PAIR(5));
        mvwprintw(board_win, cars[stopping_positions[i]].y, cars[stopping_positions[i]].x, "o-o");
        wattroff(board_win, COLOR_PAIR(5));
    }
}

//Move all cars
void move_hostile_cars(Car *cars, GameConfig config, int *hostile_positions, int game_ticks)
{
    int cols = config.playing_area_width; // szerokosc planszy

    for (int i = 0; i < config.number_of_hostile_cars; i++) // wszystkie wrogie samochody
    {
        int temp = hostile_positions[i]; // y tego samochodu

        if (game_ticks % cars[temp].speed == 0) // poruszanie sie co odpowiedni czas w zaleznosci od swojej predkosci
        {
            if (cars[temp].x <= 1 || cars[temp].x + 3 >= cols - 1) // sprawdzamy krawedzie planszy
            {
                if (get_random_number(1, 4) == 1) // 25% szans ze zmieni predkosc
                {
                    cars[temp].speed = get_random_number(1, 3); // losuje nowa predkosc
                }
                cars[temp].direction *= -1; // jak dotarl do krawedzi zmienia kierunek
            }
            cars[temp].x += cars[temp].direction; // poruszamy samochod
        }
    }
}

void move_friendly_cars(Car *cars, GameConfig config, int *friendly_positions, int game_ticks, Frog *frog, PushingCar *push_car)
{
    int cols = config.playing_area_width; // szerokosc planszy

    for (int i = 0; i < config.number_of_friendly_cars; i++) // wszystkie przyjazne samochody
    {
        int temp = friendly_positions[i]; // y tego przyjaznego

        if (push_car->waiting_to_push && push_car->car_index == temp) // jesli samochod akurat czeka na przesuniecie i jego y sie zgadza
        {
            if (!(
                    (frog->y == cars[temp].y && frog->x >= cars[temp].x && frog->x <= cars[temp].x + 2) ||
                    (frog->y + 1 == cars[temp].y && frog->x >= cars[temp].x && frog->x <= cars[temp].x + 2))) //sprawdzenie czy zaba jest w odpowiedniej pozycji na przepchniecie
            {
                push_car->waiting_to_push = 0; // jesli nie to resetujemy push_car
                push_car->car_index = -1;
            }
            else
            {
                continue; // jesli tak to lecimy
            }
        }

        if (game_ticks % cars[temp].speed == 0) // poruszanie sie co odpowiedni czas w zaleznosci od swojej predkosci
        {
            cars[temp].x += cars[temp].direction; // poruszamy samochod

            if (cars[temp].x <= 1 || cars[temp].x + 3 >= cols - 1) // sprawdzanie granicy planszy
            {
                if (get_random_number(1, 4) == 1) // 25% szans na zmiane predkosci
                {
                    cars[temp].speed = get_random_number(1, 3);
                }

                if (get_random_number(1, 2) == 1) // 50% szans na pojawienie sie z drugiej strony planszy
                {
                    if (cars[temp].x >= cols - 3) // teleportacja
                    {
                        cars[temp].x = 1;
                    }
                    else if (cars[temp].x < 1) // teleportacja
                    {
                        cars[temp].x = cols - 4;
                    }
                }
                else
                {
                    cars[temp].direction *= -1; // zmiana kierunku
                }
            }
        }
    }
}

void move_stopping_cars(Car *cars, GameConfig config, int *stopping_positions, int game_ticks, Frog *frog)
{
    int cols = config.playing_area_width; // szerokosc planszy

    for (int i = 0; i < config.number_of_stopping_cars; i++) // wszystkie stopping cars
    {
        int temp = stopping_positions[i]; // y tego aktualnego stopping

        // Calculate distance to frog
        int distance1 = abs(cars[temp].x - frog->x) + abs(cars[temp].y - frog->y); // liczenie odleglosci1
        int distance2 = abs(cars[temp].x - frog->x + 2) + abs(cars[temp].y - frog->y); // liczenie odleglosci2

        if (distance1 <= StoppingDistance || distance2 <= StoppingDistance) // jesli distance odpowiednio maly
        {
            cars[temp].is_static = true; // zatrzymujemy samochod
            continue; // samochod nie moze sie ruszyc
        }
        else
        {
            cars[temp].is_static = false; // jezdzi dalej
        }

        if (game_ticks % cars[temp].speed == 0) // poruszanie sie co odpowiedni czas w zaleznosci od swojej predkosci
        {
            cars[temp].x += cars[temp].direction; // poruszamy samochod
            if (get_random_number(1, 4) == 1) // 25% szans na zmiane predkosci
            {
                cars[temp].speed = get_random_number(1, 3); // zmiana predkosci
            }
        }

        if (cars[temp].x >= cols - 3) // teleportacja
        {
            cars[temp].x = 1;
        }
        else if (cars[temp].x < 1) // teleportacja
        {
            cars[temp].x = cols - 4;
        }
    }
}


//Move cars based on speed and direction
void move_cars(Car *cars, GameConfig config, int *hostile_positions, int *friendly_positions, int *stopping_positions, int game_ticks, Frog *frog, PushingCar *push_car)
{
    move_hostile_cars(cars, config, hostile_positions, game_ticks);
    move_friendly_cars(cars, config, friendly_positions, game_ticks, frog, push_car);
    move_stopping_cars(cars, config, stopping_positions, game_ticks, frog);

}

//Check collision between frog and hostile cars
bool check_collision_hostile_car(Frog *frog, Car *cars, GameConfig config, int *hostile_positions)
{
    for (int i = 0; i < config.number_of_hostile_cars; i++) //sprawdzenie wszystkich wrogich samochodow
    {
        int car_nr = hostile_positions[i]; // y tego aktualnego wrogiego
        if ((frog->y == cars[car_nr].y && frog->x == cars[car_nr].x) ||
            (frog->y == cars[car_nr].y && frog->x == cars[car_nr].x + 2) ||
            (frog->y + 1 == cars[car_nr].y && frog->x == cars[car_nr].x) ||   //warunki czy to kolizja
            (frog->y + 1 == cars[car_nr].y && frog->x == cars[car_nr].x + 2))
        {
            return true;
        }
    }
    return false;
}

//Moves frog by friendly cars
void move_frog_by_car(Frog *frog, Car *cars, GameConfig config, int *friendly_positions, Obstacle *obstacles, PushingCar *push_car)
{
    int tmp = FriendlyPushDistance; // jak daleko zaba moze byc popchnieta

    for (int i = 0; i < config.number_of_friendly_cars; i++)
    {
        int car_nr = friendly_positions[i]; // indeks samochodu ktory moze poruszyc zabe
        if ((frog->y == cars[car_nr].y && frog->x == cars[car_nr].x) ||
            (frog->y == cars[car_nr].y && frog->x == cars[car_nr].x + 2) || // warunki zeby zaba moga zostac poruszona przez samochod
            (frog->y + 1 == cars[car_nr].y && frog->x == cars[car_nr].x) ||
            (frog->y + 1 == cars[car_nr].y && frog->x == cars[car_nr].x + 2))
        {

            // Determine the direction of the car
            int push_direction = cars[car_nr].direction;

            for (int j = 0; j < ObstacleNumber; j++)
            {
                if (frog->y == obstacles[j].y && frog->x == obstacles[j].x - tmp ||
                    frog->y + 1== obstacles[j].y && frog->x == obstacles[j].x - tmp ||
                    frog->y == obstacles[j].y && frog->x == obstacles[j].x + tmp || //sprawdzamy czy w okol zaby nie ma zadnej przeszkody w odleglosci tmp
                    frog->y + 1== obstacles[j].y && frog->x == obstacles[j].x + tmp)
                {
                    if (push_direction == 1) // jesli tak to poruszamy ja inaczej zeby uniknac przeszkody
                    {
                        frog->x += tmp - 1;
                        continue;
                    }
                    if (push_direction == -1)
                    {
                        frog->x -= tmp + 1;
                    }
                }
            }

            //Move frog in the direction the car is moving
            if (push_direction == 1)
            { //Moving right
                if (frog->x + tmp < config.playing_area_width - 1) // sprawdzamy czy nie wychodzi poza plancze
                {
                    frog->x += tmp;
                }
                else
                {
                    frog->x = config.playing_area_width - 2;
                }
            }
            else if (push_direction == -1)
            { //Moving left
                if (frog->x - tmp > 1) // tez sprawdzamy czy nie wychodzi poza plansze
                {
                    frog->x -= tmp;
                }
                else
                {
                    frog->x = 1;
                }
            }
        }
    }
}

//Check collision between frog and friendly cars
void check_collision_friendly_car(Frog *frog, Car *cars, GameConfig config, int *friendly_positions, Obstacle *obstacles, PushingCar *push_car)
{
    for (int i = 0; i < config.number_of_friendly_cars; i++) // wszystkie friendly samochody
    {
        int car_nr = friendly_positions[i]; // y tego aktualnego

        if ((frog->y == cars[car_nr].y && frog->x == cars[car_nr].x) ||
            (frog->y == cars[car_nr].y && frog->x == cars[car_nr].x + 2) ||
            (frog->y + 1 == cars[car_nr].y && frog->x == cars[car_nr].x) ||     // warunki do sprawdzenia czy zaba jest w kolizji z samochodem
            (frog->y + 1 == cars[car_nr].y && frog->x == cars[car_nr].x + 2))
        {
            push_car->waiting_to_push = 1; // jesli tak to ustawiamy 1 - czyli ze tak
            push_car->car_index = car_nr; // podajemy y tego samochodu

            return;
        }
    }
    //Reset if no collision
    push_car->waiting_to_push;
    push_car->car_index = -1;
}



//FROG FUNCTIONS

//Create frog at starting position
void create_frog(Frog *frog, int BoardHeight, int BoardWidth)
{ // Wspolrzedne dla wyrysowania zaby
    frog->x = BoardWidth / 2;
    frog->y = BoardHeight - 3;
}

//Check if frog can jump based on delay
bool frog_jump_delay()
{
    static struct timeval last_jump_time = {0, 0}; // przechowywanie czasu ostatniego skoku statynie by nie mogla byc zmieniona
    struct timeval current_time;
    gettimeofday(&current_time, NULL); //pobranie czasu systemowego i zapisanie go w current_time

    double elapsed_time = (current_time.tv_sec - last_jump_time.tv_sec) + // obliczenie czasu od ostatniego skoku
                          (current_time.tv_usec - last_jump_time.tv_usec) / 1000000.0;

    if (elapsed_time >= JumpDelay) // jesli czas ktory uplynal jest wiekszy od opoznienia to mozemy ruszyc zaba
    {
        last_jump_time = current_time; // na nowo nadajemy czas ostatniemu skokowi
        return true;
    }
    return false;
}

//Move frog based on input direction
void frog_move(Frog *frog, int dir, GameConfig config, Obstacle *obstacle)
{
    if (!frog_jump_delay()) // sprawdzanie czy uplynelo juz wystarczajaco czasu na ruch
    {
        return;
    }

    int new_x = frog->x;
    int new_y = frog->y;

    if (dir == KEY_UP)
    {
        if (frog->y > 1)
        {
            frog->y--;
        }
    }
    if (dir == KEY_DOWN)
    {
        if (frog->y < config.playing_area_height - 3)
        {
            frog->y++;
        }
    }
    if (dir == KEY_LEFT)
    {
        if (frog->x > 1)
        {
            frog->x--;
        }
    }
    if (dir == KEY_RIGHT)
    {
        if (frog->x < config.playing_area_width - 2)
        {
            frog->x++;
        }
    }

    if (check_collision_obstacle(frog, obstacle, ObstacleNumber))
    {
        frog->x = new_x;
        frog->y = new_y;
    }
}

//Draw frog on the board
void draw_frog(WINDOW *board_win, Frog *frog)
{
    wattron(board_win, COLOR_PAIR(1)); // wlacz kolor dla board_win
    mvwprintw(board_win, frog->y, frog->x, "O"); //rysujemy zabe
    mvwprintw(board_win, frog->y + 1, frog->x, "O");
    wattroff(board_win, COLOR_PAIR(1)); //wylacz kolor dla board_win
}

//Delete previous frog from the board after move
void delete_frog(WINDOW *board_win, Frog *frog)
{
    wattron(board_win, COLOR_PAIR(3));
    mvwprintw(board_win, frog->y, frog->x, " ");
    mvwprintw(board_win, frog->y + 1, frog->x, " ");
    wattroff(board_win, COLOR_PAIR(3));
}



//Gameplay functions

bool process_user_input(WINDOW *board_win, Frog *frog, Car *car, GameConfig config, int *friendly_positions, Obstacle *obstacle, PushingCar *push_car)
{
    int move_input = wgetch(board_win); //przetrzymuje znak wprowadzony przez uzytkownika w oknie board_win

    //End game
    if (move_input == 'o')
    {
        return true;
    }

    //Check if friendly car should wait for input
    if (push_car->waiting_to_push) // friendly samochod czeka na klikniecie 'e' by moc sie przesunac
    {
        if (move_input == 'e')
        {
            move_frog_by_car(frog, car, config, friendly_positions, obstacle, push_car);

            push_car->waiting_to_push = 0; // aktualizujemy samochod ze juz ruszony
            push_car->car_index = -1; // cofamy indeks samochodu na zaden

        }
    }

    if (move_input != ERR) // o ile wejscie nie bylo bledne mozemy ruszyc zabe
    {
        frog_move(frog, move_input, config, obstacle);
    }

    return false;
}

bool refresh_screen(WINDOW *board_win, int *used_flags, Frog *frog, Car *car, GameConfig config,
                    int *hostile_positions, int *friendly_positions, int *stopping_positions,
                    Obstacle *obstacle, Finish *finish, PushingCar *push_car, time_t start_time,
                    struct timeval &last_refresh, struct timeval &current_time)
{
    long elapsed_refresh = (current_time.tv_sec - last_refresh.tv_sec) * 1000000L + // sprawdzanie czasu od osatatniego odswierzenia ekranu
                               (current_time.tv_usec - last_refresh.tv_usec);
    if (elapsed_refresh >= RefreshDelay) // jesli czas od ostateniego refresha wiekszy od limitu na refresh mozemy dzialac
    {
        //All the board elements
        werase(board_win);
        draw_board(board_win);
        frogger_text(board_win);
        draw_roads(used_flags, board_win, config.playing_area_height, config.playing_area_width);
        creare_finish(board_win, finish, config);
        draw_cars(board_win, car, config, hostile_positions, friendly_positions, stopping_positions);
        draw_obstacles(board_win, obstacle, ObstacleNumber);
        draw_frog(board_win, frog);

        int elapsed_time = (int)(time(NULL) - start_time); // oblicza czas gry w sekundach jako roznice pomiedzy aktualnym stanem a rozpoczeciem gry
        display_info(board_win, elapsed_time, config); // wyswietlenie tej informacji

        if (check_collision_hostile_car(frog, car, config, hostile_positions))
        {
            delete_frog(board_win, frog); //usuwaj zabe
            wrefresh(board_win); //odswierz zmiany
            display_lose_message(board_win); // wyswietl komunikat o przegranej
            return true;
        }

        //Check collisions with friendly cars
        check_collision_friendly_car(frog, car, config, friendly_positions, obstacle, push_car);

        if (check_finish_collision(frog, finish)) // czy zaba doszla do mety
        {
            display_win_message(board_win, elapsed_time, config); // wyswietl win info
            return true;
        }

        wrefresh(board_win); // odswierz zmiany w board_win
        refresh(); // odsierza caly terminal
        last_refresh = current_time; // pobiera czas najnowszego odswierzenia
    }
    return false;
}

void initialize_game_elements(Frog *frog, Car *car, Obstacle *obstacle, int *used_flags, GameConfig *config)
{
    initialize_flags(used_flags, config->playing_area_height);
    get_random_road(used_flags, config->playing_area_height);
    initialize_cars(car, used_flags, config->playing_area_height, config->playing_area_width);
    initialize_obstacle(obstacle, used_flags, config->playing_area_height, config->playing_area_width);
    create_frog(frog, config->playing_area_height, config->playing_area_width);
}

WINDOW *initialize_ncurses(GameConfig config)
{
    initscr(); //wlacza biblioteke ncurses
    curs_set(FALSE); //ukrycie kursora
    noecho(); //ukrycie wprowadzanych danych
    initialize_colors(); //ustawienie kolorow

    //Create board
    WINDOW *board_win = create_board(config.playing_area_height, config.playing_area_width);
    if (board_win == nullptr) // jak sie nie uda to konczymy ncurses
    {
        endwin();
    }
    keypad(board_win, TRUE); // wlacza obsluge klawiatury w oknie boardwin
    keypad(stdscr, TRUE); // wlacza obsluge klawiatury w glownym oknie

    return board_win; // zwraca wskaznik okna do wyswietlenia
}

void draw_initial_state(WINDOW *board_win, Frog *frog, Car *car, Obstacle *obstacle, int *used_flags,
                        GameConfig *config, int *hostile_positions, int *friendly_positions,
                        int *stopping_positions, Finish *finish)
{
    wclear(board_win); // zamyka wszystko co bylo dotychczas w oknie

    draw_board(board_win);
    creare_finish(board_win, finish, *config);
    draw_roads(used_flags, board_win, config->playing_area_height, config->playing_area_width);
    draw_cars(board_win, car, *config, hostile_positions, friendly_positions, stopping_positions);
    draw_obstacles(board_win, obstacle, ObstacleNumber);
    draw_frog(board_win, frog);

    wrefresh(board_win); //odswierzenie okna board_win wraz z jego wszystkimi zmianami
}

void gameplay(int *used_flags, Frog *frog, GameConfig config, WINDOW *board_win, Car *car,
              int *hostile_positions, int *friendly_positions, int *stopping_positions, Obstacle *obstacle, PushingCar *push_car)
{
     nodelay(board_win, TRUE); // ustawiamy okno tak ze nie czeka na wejscie uzytkownika
     int game_ticks = 0; // zmienna do zliczania liczby klatek gry, jest ona potrzebna do poruszania samochodami

     struct timeval last_car_move, last_refresh, current_time; // struktury do pomiaru czasu, gdzie ktore zdarzenia mialy miejsce

     gettimeofday(&last_car_move, NULL); // pobieranie bierzacego czasu dla ostatniego ruchu samochodow
     gettimeofday(&last_refresh, NULL); // pobieranie bierzacego czasu dla ostatniego odswiezania ekranu
     time_t start_time = time(NULL); // czas rozpoczecia gry w sekundach

     Finish finish;

     while (true)
     {
         gettimeofday(&current_time, NULL); // pobieranie bierzacego czasu w mikrosekundach w celu policzenia opoznienia

         if (process_user_input(board_win, frog, car, config, friendly_positions, obstacle, push_car)) // sprawdza czy jakakolwiek akcja zostala wprowadzona od uzytkownika i czy zakonczyl gre
         {
             break;
         }
        // ile minelo mikrosekund od ostatniego ruchu samochodow
         long elapsed_car_move = (current_time.tv_sec - last_car_move.tv_sec) * 1000000L +
                        (current_time.tv_usec - last_car_move.tv_usec);
         if (elapsed_car_move >= FrameDelay) // sprawdzamy czy minal odpowiedni czas od ostatniego ruchu samochodem, jesli tak wykonujemy ruch samochodow
         {
             move_cars(car, config, hostile_positions, friendly_positions, stopping_positions, game_ticks, frog, push_car);
             game_ticks++; // zwiekszamy licznik klatek gry
             last_car_move = current_time; // aktualizujemy czas ostatniego ruchu samochodem
         }

         if (refresh_screen(board_win, used_flags, frog, car, config, hostile_positions, friendly_positions, stopping_positions,
                           obstacle, &finish, push_car, start_time, last_refresh, current_time)) // odswierzenie ekranu, wyrysowanie wszystkich zaktualizowanych elementow, sprawdzenie czy gra powinna zostac zakonczona
         {
             break; // jesli gra zakonczona, wychodzimy z petli
         }
     }
}


int main(int argc, char *argv[])
{
    GameConfig config; //przechowuje konfigurajce gry

    //Zaladowanie kofuguracji gry
    if (load_config("config.txt", &config) != 0)
    {
        return 1;
    }

    //Zadeklarowanie wszystkich potrzebnych zmiennych
    Frog frog;
    Car car[RoadNumber];
    Obstacle obstacle[ObstacleNumber];
    int used_flags[config.playing_area_height];
    int hostile_positions[config.number_of_hostile_cars] = {0};
    int friendly_positions[config.number_of_friendly_cars] = {0};
    int stopping_positions[config.number_of_stopping_cars] = {0};
    PushingCar push_car = {0, -1};
    Finish finish;

    initialize_game_elements(&frog, car, obstacle, used_flags, &config);

    WINDOW *board_win = initialize_ncurses(config);
    if (board_win == nullptr) // jesli zwrocilo nullptr to konczymy
    {
        return 1;
    }

    //Draw initial game state
    draw_initial_state(board_win, &frog, car, obstacle, used_flags, &config,
                       hostile_positions, friendly_positions, stopping_positions, &finish);

    //Start gameplay loop
    gameplay(used_flags, &frog, config, board_win, car, hostile_positions, friendly_positions,
             stopping_positions, obstacle, &push_car);

    delwin(board_win); // usuwa okno board_win z pamieci
    endwin(); // konczy dzialanie ncurses

    return 0;
}