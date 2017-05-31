#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

enum lift_state_t {
	lift_state_idle,
	lift_state_open,
	lift_state_close,
	lift_state_moving
};

static void print_usage(char const *program_name) {
	std::cerr << "Usage: " << program_name <<
		" <n_floors from 5 to 20> <floor_height> <lift_speed> <close_door_delay>"
		<< std::endl;
}

static std::chrono::time_point<std::chrono::system_clock> start_point = std::chrono::system_clock::now();
static void print_seconds_from_start() {
	auto current = std::chrono::system_clock::now();
	std::chrono::duration<double> diff = current - start_point;

	std::cout << "[" << std::fixed
	          << std::setw(10)
	          << std::setfill('0')
	          << std::setprecision(3)
	          << diff.count() << "] ";
}

int main(int argc, char **argv) {
	if (argc != 5) {
		print_usage(argv[0]);
		return 1;
	}

	int n_floors = atoi(argv[1]);
	double floor_height = atof(argv[2]);
	double lift_speed = atof(argv[3]);
	double close_delay = atof(argv[4]);
	double floor_time = floor_height / lift_speed;

	if (n_floors < 5 || n_floors > 20) {
		print_usage(argv[0]);
		return 1;
	}

	int current_floor = 1;
	int destination_floor = 1;
	lift_state_t lift_state = lift_state_idle;
	while (std::cin.good()) {
		switch (lift_state) {
			case lift_state_idle: {
				print_seconds_from_start();
				std::cout << "lift is waiting for command" << std::endl;
				int n = 0;
				std::cin >> n;
				if (n < 1 || n > n_floors) continue;
				
				if (current_floor == n) {
					lift_state = lift_state_open;
				} else {
					lift_state = lift_state_moving;
					destination_floor = n;
				}
			}
			break;
			
			case lift_state_open:
				print_seconds_from_start();
				std::cout << "lift opened the door" << std::endl;
				std::this_thread::sleep_for(
					std::chrono::duration<double>(close_delay));
				lift_state = lift_state_close;
			break;
			
			case lift_state_close:
				print_seconds_from_start();
				std::cout << "lift closed the door" << std::endl;
				lift_state = lift_state_idle;
			break;
			
			case lift_state_moving:
				std::this_thread::sleep_for(
					std::chrono::duration<double>(floor_time));
				if (current_floor < destination_floor) current_floor++;
				else if (current_floor > destination_floor) current_floor--;
				print_seconds_from_start();
				std::cout << "lift arrived to " << current_floor << " floor" << std::endl;
				if (current_floor == destination_floor) {
					lift_state = lift_state_open;
				}
			break;
		}
	}

	print_seconds_from_start();
	std::cout << "lift program is terminated" << std::endl;

	return 0;
}
