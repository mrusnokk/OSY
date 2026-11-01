#ifndef ZOO_MAP_H
#define ZOO_MAP_H

void map_init(int width, int height);
void draw_map(void);
void update_map(int px, int py, const char *area_str);

#endif
