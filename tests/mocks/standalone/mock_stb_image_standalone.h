#ifndef MOCK_STB_IMAGE_STANDALONE_H
#define MOCK_STB_IMAGE_STANDALONE_H

#include <stdio.h>

/* Control API */
void mock_stbi_set_toctou_simulation(int enable);
void mock_stbi_set_info_dimensions(int width, int height, int channels);

#endif /* MOCK_STB_IMAGE_STANDALONE_H */
