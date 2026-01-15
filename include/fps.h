#ifndef FPS_H
#define FPS_H

typedef struct {
	double average_frame_time;
	float decay_factor;
	double last_log_time;
	double log_interval;
} FpsCounter;

void fps_init(FpsCounter* fps, float decay, double log_interval);
void fps_update(FpsCounter* fps, double delta_time, double current_time);

#endif