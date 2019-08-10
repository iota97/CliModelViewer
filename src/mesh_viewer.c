/* CLI rasterization - ANSI C */

/* Build with: 

"cc -O2 mesh_viewer.c -o mesh_viewer" for normal mode
"cc -O2 mesh_viewer.c -o mesh_viewer -lncurses -DNCURSES" for NCURSES mode

*/

/* Input-output and memory management headers */
#include <stdio.h>
#include <stdlib.h>

/* Ncurses header */
#ifdef NCURSES
#include <curses.h>
#endif

/* Benchmark render time */
#ifdef BENCHMARK
#include <sys/time.h>
#endif

/* Math const */
#define PI 3.14159265359f

/* Rendering const */
#define NEAR_PLANE 0.2f
#define START_Z 5.f

/* Font width/height rateo */
#define FONT_RATEO 0.5f

/* Ncurses motion step */
#ifdef NCURSES
#define TRANSLATE_STEP 0.06f
#define ROTATE_STEP 0.08f
#define SCALE_STEP 1.1f

/* Using color, Ncurses only */
static int use_color = 0;

/* Help message Ncurses */
static const char* const HELP_MESSAGE =
"Command list:								\n\
									\n\
	Move:		W - up		A - left	Z - forward	\n\
			S - down	D - right	X - backward	\n\
									\n\
	Rotate: 	I, K - on X axis				\n\
			J, L - on Y axis				\n\
			U, O - on Z axis				\n\
									\n\
	Scale:		+, -						\n\
									\n\
	Misc: 		R - reset	C - color	P - ortho view	\n\
			H - help	Q - quit			\n\
									\n\
Press ANY key to continue";					

#else
/* Help message CLI*/
static const char* const HELP_MESSAGE0 =
"Command syntax:							\n\
									\n\
	t[axis] [ammount] - translate					\n\
	r[axis] [ammount] - rotate					\n\
	s[axis] [ammount] - scale					\n\
	p - ortho view							\n\
	h - help							\n\
	m - reset							\n\
	q - quit							\n\
	v [widht]x[height] - set viewport size				\n";

static const char* const HELP_MESSAGE1 =
"	axis: x, y, z, a - all (scale only) 				\n\
									\n\
	ammount: float value						\n\
									\n\
	Examples: 'tx -0.2' translate on x axis by -0.2 		\n\
		  'ry 90' rotate on y axis by 90 degree			\n\
		  'sa 0.5' scale all the axis by half			\n\
		  'v 80x24' set vieport to 80x24 (default)		\n\
									\n\
	Press [Enter] to repeat the last command			\n\
									\n\
Press ENTER to continue							";

/* CLI start screen size */
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#endif

/* Function prototype */
float normalized_angle(float x);
float sine(float x);
float cosine(float x);
int parse_obj(char* path);
void translate(float x, float y, float z);
void update_transform(float update[3][3]);
void scale(float x, float y, float z);
void rotate_x(float x);
void rotate_y(float y);
void rotate_z(float z);
void clear_buffer(void);
void restore_mesh(void);
void render_to_buffer(void);
void clear_screen(void);
void draw(void);
void show_help(void);
void create_buffer(int width, int height);
void loop_input(void);

/* Vertex stuct */
typedef struct vertex 
{
	float x;
	float y;
	float z;
} vertex_t;

/* Tris and vertex buffer */
static int tris_count = 0;
static int *tris_buffer = NULL;
static vertex_t* vertex_buffer = NULL;

/* Screen and depth buffer */
static int buffer_width = 0;
static int buffer_height = 0;
static char *screen = NULL;
static float *depth = NULL;

/* Screen rateo based on screen height, width and font rateo */
static float screen_rateo;

/* Orthographic or perspective */
static int ortho = 0;

/* Material array of char */
static char material_array[] = {'a', 'b', 'c', 'd', 
				'e', 'f', 'g', 'h',
				'i', 'j', 'k', 'l',
				'm', 'n', 'o', 'p'};

/* Transform matrix */
static float transform[4][4];

/* Normalize rotation between 0 and 2PI */
float normalized_angle(float x)
{
	return (x < 0) ? x + 2*PI*((int)(-x/(2*PI))+1) : x - 2*PI*(int)(x/(2*PI));
}	

/* Sine */
float sine(float x)
{
	float sign;

	/* Normalize the rotation */
	x = normalized_angle(x);
		
	/* Check sign */
	if (x < PI)
		sign = 1;
	else
		sign = -1;

	/* Check symmetry */
	if (x < PI/2 || (x >= PI && x < 3*PI/2))
		x -= (PI/2)*(int)(x/(PI/2));
	else
		x = PI/2 - x + (PI/2)*(int)(x/(PI/2));

	/* Check it to be below 45 degree */
	if (x < PI/4)
	{
		/* Polinomial approximation */
		float x2 = x*x;
		float x3 = x2*x;
		float sine = x - x3/6 + x2*x3/120;

		return sine*sign;
	}
	else
	{
		float x2, x4, x6, cosine;

		/* Transform to cosine */
		x = PI/2 - x;

		/* Polinomial approximation */
		x2 = x*x;
		x4 = x2*x2;
		x6 = x2*x4;
		cosine = 1 - x2/2 + x4/24 - x6/720;

		return cosine*sign;
	}
}

/* Cosine */
float cosine(float x)
{
	float sign;

	/* Normalize the rotation */
	x = normalized_angle(x);
		
	/* Check sign */
	if (x < PI/2 || x >= 3*PI/2)
		sign = 1;
	else
		sign = -1;

	/* Check symmetry */
	if (x < PI/2 || (x >= PI && x < 3*PI/2))
		x -= (PI/2)*(int)(x/(PI/2));
	else
		x = PI/2 - x + (PI/2)*(int)(x/(PI/2));

	/* Check it to be below 45 degree */
	if (x < PI/4)
	{
		/* Polinomial approximation */
		float x2 = x*x;
		float x4 = x2*x2;
		float x6 = x2*x4;
		float cosine = 1 - x2/2 + x4/24 - x6/720;

		return cosine*sign;
	}
	else
	{
		float x2, x3, sine;
	
		/* Transform to sine */
		x = PI/2 - x;

		/* Polinomial approximation */
		x2 = x*x;
		x3 = x2*x;
		sine = x - x3/6 + x2*x3/120;

		return sine*sign;
	}
}

/* Read the mesh file */
int parse_obj(char* path) 
{
	static unsigned int vertex_count = 0;
	char line_buffer[1024];
	FILE* mesh_file = fopen(path, "r");

	/* Check the read to be succefull */
	if (mesh_file == NULL)
	{
		printf("Error reading file %s\n", path);
		return 1;
	}	

	/* Cycle the file once to get the vertex and tris count */
	while(fgets(line_buffer, 1024, mesh_file)) 
	{
		/* Count vertex */
		if (line_buffer[0] == 'v' && line_buffer[1] == ' ') 
			vertex_count++;
		
		/* Count tris */
		else if (line_buffer[0] == 'f' && line_buffer[1] == ' ')
		{
			int test_vertex;

			/* Parse the first tris */
			sscanf(line_buffer, "%*s %*s %*s %*s %[^\n]", line_buffer);

			/* Increase tris count */
			tris_count++;
			
			/* Parse other tris if the face have more vertex */
			while (sscanf(line_buffer, "%d/", &test_vertex) == 1)
			{
				/* Increase tris count */
				tris_count++;

				/* Remove already parsed face and keep looping if other are present */
				if (sscanf(line_buffer, "%*s %[^\n]", line_buffer) != 1)
					break;
			}
		}
	}

	/* Check vertex and tris count */
	if (vertex_count == 0 || tris_count == 0)
	{
		printf("Corrupted file %s\n", path);
		return 1;
	}

	/* Free previous tris and vertex */
	free(tris_buffer);
	free(vertex_buffer);

	/* Alloc the memory */
	vertex_buffer = (vertex_t*) malloc(vertex_count * sizeof(vertex_t));
	tris_buffer = (int*) malloc(tris_count * sizeof(int) * 3);

	/* Reset the counter and rewind the file */
	vertex_count = 0;
	tris_count = 0;
	rewind(mesh_file);

	/* Cycle again, reading the data */
	while(fgets(line_buffer, 1024, mesh_file)) 
	{
		/* Read vertex data */
		if (line_buffer[0] == 'v' && line_buffer[1] == ' ') 
		{
			sscanf(line_buffer, "%*s %f %f %f", 
				&vertex_buffer[vertex_count].x, 
				&vertex_buffer[vertex_count].y, 
				&vertex_buffer[vertex_count].z);

			vertex_count++;
		}

		/* Read face data */
		else if (line_buffer[0] == 'f' && line_buffer[1] == ' ') 
		{
			/* Parse the first tris */
			int vertex0, vertex1, vertex2;
			sscanf(line_buffer, "%*s %d/%*s %d/%*s %d/%*s %[^\n]",
				&vertex0, &vertex1, &vertex2, line_buffer);

			/* Bind the vertex */
			tris_buffer[tris_count*3+0] = vertex0-1;
			tris_buffer[tris_count*3+1] = vertex1-1;
			tris_buffer[tris_count*3+2] = vertex2-1;

			/* Increase tris count */
			tris_count++;
			
			/* Parse other tris if the face have more vertex */
			while (sscanf(line_buffer, "%d/", &vertex1) == 1)
			{
				/* Bind the vertex0, the new vertex and the last as a tris */
				tris_buffer[tris_count*3+0] = vertex0-1;
				tris_buffer[tris_count*3+1] = vertex1-1;
				tris_buffer[tris_count*3+2] = vertex2-1;

				/* Set vertex2 as our new last vertex */
				vertex2 = vertex1;			
				
				/* Increase tris count */
				tris_count++;

				/* Remove already parsed face and keep looping if other are present */
				if (sscanf(line_buffer, "%*s %[^\n]", line_buffer) != 1)
					break;
			}
		}
	}

	/* Close the file */
	fclose(mesh_file);

	return 0;
}

/* Translate the mesh */
void translate(float x, float y, float z) 
{
	/* Add translation directly to the matrix */
	transform[3][0] -= x;
	transform[3][1] += y;
	transform[3][2] += z;

	return;
}

/* Update transform matrix for scale and rotation */
void update_transform(float update[3][3])
{
	int i, j, k;

	/* Copy old transform */
	float copy[4][4];
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			copy[i][j] = transform[i][j];
		}
	}

	/* Multiply the update * transform using the copy */
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			transform[i][j] = 0;

			for (k = 0; k < 3; k++)
			{
				transform[i][j] += update[k][j]*copy[i][k];
			}
		}
	}

	return;
}

/* Scale the mesh */
void scale(float x, float y, float z)
{
	int i, j;
	float update[3][3];

	/* Create empty matrix */
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			update[i][j] = 0;
		}
	}

	/* Add scale */
	update[0][0] = x;
	update[1][1] = y;
	update[2][2] = z;

	/* Update global matrix */
	update_transform(update);

	return;
}

/* Rotate the mesh on X axis */
void rotate_x(float x)
{
	int i, j;
	float update[3][3];

	/* Create empty matrix */
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			update[i][j] = 0;
		}
	}

	/* Add rotation */
	update[1][1] = cosine(x);
	update[1][2] = sine(x);
	update[2][2] = update[1][1];
	update[2][1] = -update[1][2];
	update[0][0] = 1.f;

	/* Update global matrix */
	update_transform(update);

	return;
}

/* Rotate the mesh on Y axis */
void rotate_y(float y)
{
	int i, j;
	float update[3][3];

	/* Create empty matrix */
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			update[i][j] = 0;
		}
	}

	/* Add rotation */
	update[0][0] = cosine(y);
	update[2][0] = -sine(y);
	update[0][2] = -update[2][0];
	update[2][2] = update[0][0];
	update[1][1] = 1.f;

	/* Update global matrix */
	update_transform(update);

	return;
}

/* Rotate the mesh on Z axis */
void rotate_z(float z)
{
	int i, j;
	float update[3][3];

	/* Create empty matrix */
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			update[i][j] = 0;
		}
	}

	/* Add rotation */
	update[0][0] = cosine(-z);
	update[0][1] = sine(-z);
	update[1][0] = -update[0][1];
	update[1][1] = update[0][0];
	update[2][2] = 1.f;

	/* Update global matrix */
	update_transform(update);

	return;
}

/* Clear the screen and depth buffer */
void clear_buffer()
{
	int i, j;
	for (i = 0; i < buffer_width; i++)
	{
		for (j = 0; j < buffer_height; j++)
		{
			screen[i+j*buffer_width] = ' ';
			depth[i+j*buffer_width] = 0;
		}
	}

	return;
}

/* Restore position */
void restore_mesh()
{
	unsigned int i, j;
	
	/* Restore identity matrix */
	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			if (i == j)
				transform[i][j] = 1;
			else
				transform[i][j] = 0;
		}
	}

	/* Translate and rotate for initial view */
	translate(0, 0, START_Z);
	rotate_y(PI);

	return;
}

/* Render to screen buffer */
void render_to_buffer()
{
	int i, j;
	int x, y;
	unsigned int material_index = 0;

	/* Used for near plane culling when we split a tris in 2 */
	int last_half_rendered = 0;

	/* Clear before start */
	clear_buffer();

	for (i = 0; i < tris_count; i++) 
	{
		/* Barycentric coordinate determinant */
		float determinant;

		/* Get the bounding coordinate of the tris */
		int min_x = buffer_width-1;
		int min_y = buffer_height-1;
		int max_x = 0;
		int max_y = 0;
		int behind_near = 0;

		/* Transformed vertex */
		vertex_t vertex[3];
	
		/* Render the tris with another material */
		material_index++;

		/* Return to 0 if last element is reached */
		if (material_index == sizeof(material_array)/sizeof(material_array[0]))
			material_index = 0;

		for (j = 0; j < 3; j++)
		{
			/* Multiply with transform matrix */
			vertex[j].x = transform[0][0]*vertex_buffer[tris_buffer[i*3+j]].x+
					transform[1][0]*vertex_buffer[tris_buffer[i*3+j]].y+
					transform[2][0]*vertex_buffer[tris_buffer[i*3+j]].z+
					transform[3][0];

			vertex[j].y = transform[0][1]*vertex_buffer[tris_buffer[i*3+j]].x+
					transform[1][1]*vertex_buffer[tris_buffer[i*3+j]].y+
					transform[2][1]*vertex_buffer[tris_buffer[i*3+j]].z+
					transform[3][1];

			vertex[j].z = transform[0][2]*vertex_buffer[tris_buffer[i*3+j]].x+
					transform[1][2]*vertex_buffer[tris_buffer[i*3+j]].y+
					transform[2][2]*vertex_buffer[tris_buffer[i*3+j]].z+
					transform[3][2];

			/* Count vertex behind near plane */
			if (vertex[j].z < NEAR_PLANE)
				behind_near++;
		}

		/* Near plane culling */
		if (behind_near > 0)
		{
			/* If all vertex are behind */
			if (behind_near == 3)
			{
				/* Skip the tris */
				continue;
			}

			/* If we have 1 or 2 vertex behind */
			else
			{
				/* Find the 2 point of intersection with the near plane */
				vertex_t intersect[2];
				int id[3];

				/* Set id[0] as id of the one on the different side */
				if ((vertex[0].z >= NEAR_PLANE && behind_near == 2) ||
					(vertex[0].z < NEAR_PLANE && behind_near == 1))
				{
					id[0] = 0;
					id[1] = 1;
					id[2] = 2;
				}
				else if ((vertex[1].z >= NEAR_PLANE && behind_near == 2) ||
					(vertex[1].z < NEAR_PLANE && behind_near == 1))
				{
					id[0] = 1;
					id[1] = 0;
					id[2] = 2;
				}
				else
				{
					id[0] = 2;
					id[1] = 0;
					id[2] = 1;
				}

				/* Calculate the intersection point, check alignment first */
				intersect[0].x = (vertex[id[0]].x == vertex[id[1]].x) ? vertex[id[0]].x :
						vertex[id[0]].x + (NEAR_PLANE-vertex[id[0]].z)*
						(vertex[id[0]].x-vertex[id[1]].x)/(vertex[id[0]].z-vertex[id[1]].z);

				intersect[0].y = (vertex[id[0]].y == vertex[id[1]].y) ? vertex[id[0]].y :
						vertex[id[0]].y + (NEAR_PLANE-vertex[id[0]].z)*
						(vertex[id[0]].y-vertex[id[1]].y)/(vertex[id[0]].z-vertex[id[1]].z);
				
				intersect[1].x = (vertex[id[0]].x == vertex[id[2]].x) ? vertex[id[0]].x :
						vertex[id[0]].x + (NEAR_PLANE-vertex[id[0]].z)*
						(vertex[id[0]].x-vertex[id[2]].x)/(vertex[id[0]].z-vertex[id[2]].z);

				intersect[1].y = (vertex[id[0]].y == vertex[id[2]].y) ? vertex[id[0]].y :
						vertex[id[0]].y + (NEAR_PLANE-vertex[id[0]].z)*
						(vertex[id[0]].y-vertex[id[2]].y)/(vertex[id[0]].z-vertex[id[2]].z);
				
				/* Create new tris */
				if (behind_near == 2)
				{
					/* The vertex in front is kept, the other 2 become the intersect */
					vertex[0].x = vertex[id[0]].x;
					vertex[0].y = vertex[id[0]].y;
					vertex[0].z = vertex[id[0]].z;

					vertex[1].x = intersect[0].x;
					vertex[1].y = intersect[0].y;
					vertex[1].z = NEAR_PLANE;

					vertex[2].x = intersect[1].x;
					vertex[2].y = intersect[1].y;
					vertex[2].z = NEAR_PLANE;
				}
				else
				{
					/* If we have 2 vertex in front we need to create 2 tris */
					last_half_rendered = !last_half_rendered;

					/* Check with tris we rendered last time */
					if (last_half_rendered)
					{
						/* Create the first tris */
						vertex_t tmp;

						tmp.x = vertex[id[1]].x;
						tmp.y = vertex[id[1]].y;
						tmp.z = vertex[id[1]].z;

						vertex[2].x = vertex[id[2]].x;
						vertex[2].y = vertex[id[2]].y;
						vertex[2].z = vertex[id[2]].z;
	
						vertex[1].x = tmp.x;
						vertex[1].y = tmp.y;
						vertex[1].z = tmp.z;

						vertex[0].x = intersect[0].x;
						vertex[0].y = intersect[0].y;
						vertex[0].z = NEAR_PLANE;

						/* Decrease i, so we get the same tris next cycle */
						i--;
					}
					else
					{	
						/* Create the second tris */
						vertex[2].x = vertex[id[2]].x;
						vertex[2].y = vertex[id[2]].y;
						vertex[2].z = vertex[id[2]].z;
				
						vertex[0].x = intersect[1].x;
						vertex[0].y = intersect[1].y;
						vertex[0].z = NEAR_PLANE;

						vertex[1].x = intersect[0].x;
						vertex[1].y = intersect[0].y;
						vertex[1].z = NEAR_PLANE;
					
						/* Decrease material, so we use the same as the other half */
						material_index = (material_index > 0) ? material_index-1 :
								sizeof(material_array)/sizeof(material_array[0])-1;
					}
				}
			}
		}

		/* Raster vertex to screen */
		for (j = 0; j < 3; j++)
		{
			/* Orthographic projection */
			if (ortho)
			{
				vertex[j].x = (vertex[j].x / -transform[3][2] * buffer_width) + buffer_width/2;
				vertex[j].y = (vertex[j].y / -transform[3][2] * buffer_height)*screen_rateo + buffer_height/2;
			}

			/* Perspective projection */
			else
			{
				vertex[j].x = (vertex[j].x / -vertex[j].z * buffer_width) + buffer_width/2;
				vertex[j].y = (vertex[j].y / -vertex[j].z * buffer_height)*screen_rateo + buffer_height/2;
			}

			/* Get boundaries */
			if (vertex[j].x < min_x)
				min_x = (int) vertex[j].x;
			if (vertex[j].x > max_x)
				max_x = (int) vertex[j].x;
			if (vertex[j].y < min_y)
				min_y = (int) vertex[j].y;
			if (vertex[j].y > max_y)
				max_y = (int) vertex[j].y;
		}

		/* Check boundaries */
		max_x = (max_x > buffer_width-1) ? buffer_width-1 : max_x;
		max_y = (max_y > buffer_height-1) ? buffer_height-1 : max_y;
		min_x = (min_x < 0) ? 0 : min_x;
		min_y = (min_y < 0) ? 0 : min_y;
		
		/* Calculate the determinant for barycentric coordinate */
		determinant = 1/((vertex[1].y-vertex[2].y)*(vertex[0].x-vertex[2].x) +
					(vertex[2].x-vertex[1].x)*(vertex[0].y-vertex[2].y));

		/* Test only the pixel in this area */
		for (y = min_y; y <= max_y; y++) 
		{
			for (x = min_x; x <= max_x; x++) 
			{
				/* Calculate barycentric coordinate */
				float lambda0 = ((vertex[1].y-vertex[2].y)*(x-vertex[2].x) +
					(vertex[2].x-vertex[1].x)*(y-vertex[2].y))*determinant;
				float lambda1 = ((vertex[2].y-vertex[0].y)*(x-vertex[2].x) +
					(vertex[0].x-vertex[2].x)*(y-vertex[2].y))*determinant;
				float lambda2 = 1.0f - lambda0 - lambda1;
				
				/* If is inside the triangle, render it */
				if (lambda0 >= 0 && lambda1 >= 0 && lambda2 >= 0) 
				{
					/* Interpolate Z value */
					float pixel_depth;
			
					/* Check if we are using ortho or perspective rendering */
					if (ortho)
					{
						pixel_depth = 1.f/(vertex[0].z * lambda0 +
								vertex[1].z * lambda1 +
								vertex[2].z * lambda2);
					}
					else
					{
						/* Perspective correct interpolation */
						pixel_depth = lambda0 / vertex[0].z +
								lambda1 / vertex[1].z +
								lambda2 / vertex[2].z;
					}

					/* Test depth buffer */
					if (depth[x+y*buffer_width] < pixel_depth)
					{
						/* Update both buffer */
						screen[x+y*buffer_width] = material_array[material_index];
						depth[x+y*buffer_width] = pixel_depth;
					}
				}
			}			
		}
	}

	return;
}

/* Clear the console */
void clear_screen()
{	
	#ifdef NCURSES
	/* Just call ncurses clear */
	clear();
	
	#else
	/* Use CLI method */
	int i;

	/* Print 64 new line */
	for (i = 0; i < 64; i++)
		putchar('\n');
	#endif

	return;
}

/* Draw in the console */
void draw()
{
	int i, j;

	/* Frame benchmark */
	#ifdef BENCHMARK
	struct timeval start_frame, stop_frame;
	gettimeofday(&start_frame, NULL);
	#endif

	/* Render to buffer */
	render_to_buffer();
	
	/* Clear the console */
	clear_screen();

	#ifdef NCURSES
	/* Print it, Ncurses color mode */
	if (use_color)
	{
		for (j = 0; j < buffer_height; j++) 
		{
			for (i = 0; i < buffer_width; i++)
			{
				/* Set color attribute */
				if (screen[i+j*buffer_width] != ' ')
					attron(COLOR_PAIR(screen[i+j*buffer_width]%7+2));
				else
					attron(COLOR_PAIR(1));
					
				/* Print a blank space */
				addch(' ');
			}
		}

		/* Restore black and white */
		attron(COLOR_PAIR(1));
	}

	/* Ncurses, no color */
	else
	{
		for (j = 0; j < buffer_height; j++) 
		{
			for (i = 0; i < buffer_width; i++)
			{
				/* Print the char in the buffer */
				addch(screen[i+j*buffer_width]);
			}
		}
	}

	/* Frame benchmark */
	#ifdef BENCHMARK
	gettimeofday(&stop_frame, NULL);

	mvprintw(0, 0, "[Frame time: %.1f ms]", 
		(double)(stop_frame.tv_usec - start_frame.tv_usec)/1000+
		(double)(stop_frame.tv_sec - start_frame.tv_sec)*1000);
	#endif

	#else
	/* Print it, CLI mode */
	for (j = 0; j < buffer_height; j++) 
	{
		for (i = 0; i < buffer_width; i++)
		{
			putchar(screen[i+j*buffer_width]);
		}
		putchar('\n');
	}

	/* Frame benchmark */
	#ifdef BENCHMARK
	gettimeofday(&stop_frame, NULL);

	printf("[Frame time: %.1f ms] > ", 
		(double)(stop_frame.tv_usec - start_frame.tv_usec)/1000+
		(double)(stop_frame.tv_sec - start_frame.tv_sec)*1000);
	#else
	printf("> ");
	#endif
	#endif

	return;
}

/* Help message */
void show_help()
{
	/* Clear the console */
	clear_screen();

	#ifdef NCURSES
	/* Print usage instruction, Ncurses mode */
	mvprintw(0, 0, "%s", HELP_MESSAGE);

	/* Wait input */
	getch();

	#else
	/* Print usage instruction, CLI mode */
	puts(HELP_MESSAGE0);
	puts(HELP_MESSAGE1);
	
	/* Wait input to proceed */
	getchar();
	#endif

	return;
}

/* Create depth and screen buffer */
void create_buffer(int width, int height)
{
	/* Check width and height to be more than zero */
	if (width <= 0 || height <= 0)
		return;

	/* Free old buffer */
	free(screen);
	free(depth);

	/* Allocate new one */
	screen = (char*) malloc(sizeof(char) * width * height);
	depth = (float*) malloc(sizeof(float) * width * height);

	/* Set global buffer size */
	buffer_width = width;
	buffer_height = height;

	/* Update screen rateo */
	screen_rateo = (float)buffer_width/buffer_height*FONT_RATEO;

	return;
}

#ifdef NCURSES
/* Ncurses input loop */
void loop_input()
{
	/* Input variables */
	int command;
	int quit = 0;

	while (!quit)
	{
		/* Check screen to be the same size as before */
		if (buffer_width != getmaxx(stdscr) || buffer_height != getmaxy(stdscr))
		{
			create_buffer(getmaxx(stdscr), getmaxy(stdscr));
		}

		/* Render */
		draw();

		/* Get input */
		command = getch();
	
		switch (command)
		{
			/* Quit */
			case 'q':
				quit = 1;
				break;

			/* Help */
			case 'h':
				show_help();
				break;

			/* Orthogonal or perspective */
			case 'p':
				ortho = !ortho;
				break;
		
			/* Color */
			case 'c':
		 		if (has_colors())
					use_color = !use_color;
				break;

			/* Move up */
			case 'w':
				translate(0, TRANSLATE_STEP, 0);
				break;

			/* Move down */
			case 's':
				translate(0, -TRANSLATE_STEP, 0);
				break;

			/* Move left */
			case 'a':
				translate(-TRANSLATE_STEP, 0, 0);
				break;
			
			/* Move right */
			case 'd':
				translate(TRANSLATE_STEP, 0, 0);
				break;

			/* Move forward */
			case 'z':
				translate(0, 0, -TRANSLATE_STEP);
				break;

			/* Move backward */
			case 'x':
				translate(0, 0, TRANSLATE_STEP);
				break;

			/* Scale up */
			case '+':
				scale(SCALE_STEP, SCALE_STEP, SCALE_STEP);
				break;

			/* Scale down */
			case '-':
				scale(1/SCALE_STEP, 1/SCALE_STEP, 1/SCALE_STEP);
				break;

			/* Rotate y */
			case 'j':
				rotate_y(ROTATE_STEP);
				break;

			case 'l':
				rotate_y(-ROTATE_STEP);
				break;

			/* Rotate x */
			case 'i':
				rotate_x(ROTATE_STEP);
				break;

			case 'k':
				rotate_x(-ROTATE_STEP);
				break;

			/* Rotate z */
			case 'u':
				rotate_z(ROTATE_STEP);
				break;
		
			case 'o':
				rotate_z(-ROTATE_STEP);
				break;

			/* Restore mesh */
			case 'r':
				restore_mesh();
				break;
		}
	}

	return;
}

#else
/* CLI input loop */
void loop_input()
{
	/* Input variables */
	char command[64];
	char last[2] = "\0";
	float ammount = 1;
	int quit = 0;

	/* Keep looping until user hit 'q' */
	while(!quit)
	{	
		/* Render it */
		draw();

		/* Get input */
		fgets(command, 64, stdin);

		/* Quit */
		if (command[0] == 'q')
			quit = 1;

		/* Orthogonal or perspective */
		else if (command[0] == 'p')
			ortho = !ortho;

		/* Viewport resize */
		else if (command[0] == 'v')
		{
			/* Try to parse width and height */
			int width, height;

			if (sscanf(command, "%*s %dx%d", &width, &height) == 2)
			{
				/* Recreate a new buffer */
				create_buffer(width, height);
			}
		}
		
		/* If just enter, restore the previous one */
		else if (command[0] == '\n')
		{
			command[0] = last[0];
			command[1] = last[1];
		}

		/* Parse the ammount */
		sscanf(command, "%*s %f", &ammount);

		/* Translation */
		if (command[0] == 't')
		{
			/* Translate */
			if (command[1] == 'x')
			{
				translate(ammount, 0, 0);
			}
			else if (command[1] == 'y')
			{
				translate(0, ammount, 0);
			}
			else if (command[1] == 'z')
			{
				translate(0, 0, ammount);
			}
		}

		/* Rotation */
		else if (command[0] == 'r')
		{
			/* Convert from degree to radian */
			float angle = ammount*PI/180;

			/* Rotate */
			if (command[1] == 'x')
			{
				rotate_x(angle);
			}
			else if (command[1] == 'y')
			{
				rotate_y(angle);
			}
			else if (command[1] == 'z')
			{
				rotate_z(angle);
			}
		}

		/* Scale */
		else if (command[0] == 's')
		{
			/* Scale */
			if (command[1] == 'x')
			{
				scale(ammount, 1, 1);
			}
			else if (command[1] == 'y')
			{
				scale(1, ammount, 1);
			}
			else if (command[1] == 'z')
			{
				scale(1, 1, ammount);
			}
			else if (command[1] == 'a')
			{
				scale(ammount, ammount, ammount);
			}
		}

		/* Reset */
		else if (command[0] == 'm')
			restore_mesh();

		/* Print help */
		else if (command[0] == 'h')
			show_help();	

		/* Save last command */
		last[0] = command[0];
		last[1] = command[1];
	}

	return;
}
#endif

/* Main */
int main(int argc, char *argv[]) 
{
	/* Check argument number */
	if (argc < 2) 
	{		
		puts("Please provide the model path.\n");
		return 1;	
	}

	/* Parse the model */
	if (parse_obj(argv[1]))
		return 2;

	/* Ncurses init */
	#ifdef NCURSES
	initscr();
	cbreak();
	noecho();
	curs_set(0);

	/* Color init */
	if (has_colors())
	{
		unsigned int i;
		start_color();
		
		/* Material array color pair*/
		for (i = 0; i < sizeof(material_array)/sizeof(material_array[0]); i++)
		{
			/* Color from 1 to 7 */
			init_pair(material_array[i]%7+2, COLOR_WHITE, i%7+1);
		}

		/* Blank space */
		init_pair(1, COLOR_WHITE, COLOR_BLACK);
	}
	#endif

	/* Print help message at start */
	show_help();

	/* Restore the mesh */
	restore_mesh();

	/* Allocate rendering buffer */
	#ifdef NCURSES
	create_buffer(getmaxx(stdscr), getmaxy(stdscr));

	#else
	create_buffer(SCREEN_WIDTH, SCREEN_HEIGHT);
	#endif

	/* Start the input loop */
	loop_input();

	/* Free memory */
	free(tris_buffer);
	free(vertex_buffer);
	free(screen);
	free(depth);
	
	/* Kill the windows */
	#ifdef NCURSES
	endwin();
	#endif

	/* Exit */
	return 0;
}
