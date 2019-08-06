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

/* Configs */
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 24
#define FONT_RATEO 0.5f

#ifdef NCURSES
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
	Misc: 		R - reset	Q - quit	H - help 	\n\
									\n\
Press ANY key to continue						";

#else
/* Help message CLI*/
static const char* const HELP_MESSAGE0 =
"Command syntax:							\n\
									\n\
	t[axis] [ammount] - translate					\n\
	r[axis] [ammount] - rotate					\n\
	s[axis] [ammount] - scale					\n\
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
#endif

/* Math const */
#define PI 3.14159265359f

/* Rendering const */
#define NEAR_PLANE 1
#define FAR_PLANE 8192

/* Function prototype */
float absolute (float x);
float normalized_angle(float x);
float sine(float x);
float cosine(float x);
void free_tris(void);
void free_buffer(void);
int parse_obj(char* path);
void translate(float x, float y, float z);
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

/* Translation */
static float translation_x = 0;
static float translation_y = 0;
static float translation_z = 0;

/* Rotation */
static float rotation_x = 0;
static float rotation_y = 0;
static float rotation_z = 0;

/* Scale */
static float scale_x = 1;
static float scale_y = 1;
static float scale_z = 1;

/* Vertex and tris counter */
static unsigned int tris_count = 0;
static unsigned int vertex_count = 0;

/* Tris Buffer, one for applying transform */
static vertex_t *tris_buffer = NULL;
static vertex_t *mesh_tris = NULL;

/* Screen and depth buffer */
static int buffer_width = SCREEN_WIDTH;
static int buffer_height = SCREEN_HEIGHT;
static char *screen = NULL;
static float *depth = NULL;

/* Calculate the screen rateo */
static float screen_rateo = SCREEN_WIDTH/SCREEN_HEIGHT*FONT_RATEO;

/* Material array of char */
static char material_array[] = {'a', 'b', 'c', 'd', 
				'e', 'f', 'g', 'h',
				'i', 'j', 'k', 'l',
				'm', 'n', 'o', 'p'};

/* Calculate the absolute value */
float absolute(float x)
{
	return x > 0 ? x : -x;
}

/* Normalize rotation between 0 and 2PI */
float normalized_angle(float x)
{
	if (x < 0)
		x += 2*PI*((int)(-x/(2*PI))+1);
	else 
		x -= 2*PI*(int)(x/(2*PI));

	return x;
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

/* Free tris memory */
void free_tris()
{ 
	/* Free memory */
	free(mesh_tris);
	free(tris_buffer);

	return;
}

/* Free buffer */
void free_buffer()
{ 
	/* Free memory */
	free(screen);
	free(depth);

	return;
}

/* Read the mesh file */
int parse_obj(char* path) 
{
	char line_buffer[1024];
	FILE* mesh_file = fopen(path, "r");
	vertex_t* vertex_buffer = NULL;

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
			while (sscanf(line_buffer, "%u/", &test_vertex) == 1)
			{
				/* Increase tris count */
				tris_count++;

				/* Remove already parsed face and keep looping if other are present */
				if (sscanf(line_buffer, "%*s %[^\n]", line_buffer) != 1)
					break;
			}
		}
	}

	/* Free previous tris  */
	free_tris();

	/* Alloc the memory */
	vertex_buffer = (vertex_t*) malloc(vertex_count * sizeof(vertex_t));
	tris_buffer = (vertex_t*) malloc(tris_count * sizeof(vertex_t) * 3);
	mesh_tris = (vertex_t*) malloc(tris_count * sizeof(vertex_t) * 3);

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
			sscanf(line_buffer, "%*s %u/%*s %u/%*s %u/%*s %[^\n]",
				&vertex0, &vertex1, &vertex2, line_buffer);

			/* Bind the vertex */
			mesh_tris[tris_count*3+0] = vertex_buffer[vertex0-1];
			mesh_tris[tris_count*3+1] = vertex_buffer[vertex1-1];
			mesh_tris[tris_count*3+2] = vertex_buffer[vertex2-1];

			/* Increase tris count */
			tris_count++;
			
			/* Parse other tris if the face have more vertex */
			while (sscanf(line_buffer, "%u/", &vertex1) == 1)
			{
				/* Bind the vertex0, the new vertex and the last as a tris */
				mesh_tris[tris_count*3+0] = vertex_buffer[vertex0-1];
				mesh_tris[tris_count*3+1] = vertex_buffer[vertex1-1];
				mesh_tris[tris_count*3+2] = vertex_buffer[vertex2-1];

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

	/* Free the vertex buffer */
	free(vertex_buffer);

	/* Close the file */
	fclose(mesh_file);

	return 0;
}

/* Translate the mesh */
void translate(float x, float y, float z) 
{
	unsigned int i, j;
	for (i = 0; i < tris_count; i++) 
	{
		for (j = 0; j < 3; j++) 
		{
			tris_buffer[i*3+j].x -= x; 
			tris_buffer[i*3+j].y += y;
			tris_buffer[i*3+j].z += z;
		}
	}

	return;
}

/* Scale the mesh */
void scale(float x, float y, float z)
{
	unsigned int i, j;
	for (i = 0; i < tris_count; i++) 
	{
		for (j = 0; j < 3; j++) 
		{
			tris_buffer[i*3+j].x *= x; 
			tris_buffer[i*3+j].y *= y;
			tris_buffer[i*3+j].z *= z;
		}

	}

	return;
}

/* Rotate the mesh on X axis */
void rotate_x(float x)
{
	unsigned int i, j;
	for (i = 0; i < tris_count; i++) 
	{
		for (j = 0; j < 3; j++) 
		{
			/* Back up float */
			float tmp = tris_buffer[i*3+j].y;
			
			tris_buffer[i*3+j].y = cosine(x) * tris_buffer[i*3+j].y -
						sine(x) * tris_buffer[i*3+j].z;
			tris_buffer[i*3+j].z = sine(x) * tmp +
						cosine(x) * tris_buffer[i*3+j].z;
		}
	}

	return;
}

/* Rotate the mesh on Y axis */
void rotate_y(float y)
{
	unsigned int i, j;
	for (i = 0; i < tris_count; i++) 
	{
		for (j = 0; j < 3; j++) 
		{
			/* Back up float */
			float tmp = tris_buffer[i*3+j].x;
			
			tris_buffer[i*3+j].x = cosine(y) * tris_buffer[i*3+j].x -
						sine(y) * tris_buffer[i*3+j].z;
			tris_buffer[i*3+j].z = sine(y) * tmp +
						cosine(y) * tris_buffer[i*3+j].z; 
		}
	}

	return;
}

/* Rotate the mesh on Z axis */
void rotate_z(float z)
{
	unsigned int i, j;
	for (i = 0; i < tris_count; i++) 
	{
		for (j = 0; j < 3; j++) 
		{
			/* Back up float */
			float tmp = tris_buffer[i*3+j].x;
			
			tris_buffer[i*3+j].x = cosine(-z) * tris_buffer[i*3+j].x -
						sine(-z) * tris_buffer[i*3+j].y;
			tris_buffer[i*3+j].y = sine(-z) * tmp +
						cosine(-z) * tris_buffer[i*3+j].y; 
		}
	}

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
			depth[i+j*buffer_width] = FAR_PLANE;
		}
	}

	return;
}

/* Restore position */
void restore_mesh()
{
	unsigned int i, j;	

	/* Restore translation, scale and rotation */
	translation_x = 0;
	translation_y = 0;
	translation_z = 5;
	rotation_x = 0;
	rotation_y = 0;
	rotation_z = 0;
	scale_x = 1;
	scale_y = 1;
	scale_z = 1;

	/* Restore the tris from the mesh one */
	for (i = 0; i < tris_count; i++) 
	{
		for (j = 0; j < 3; j++) 
		{
			tris_buffer[i*3+j].x = mesh_tris[i*3+j].x; 
			tris_buffer[i*3+j].y = mesh_tris[i*3+j].y;
			tris_buffer[i*3+j].z = mesh_tris[i*3+j].z;
		}
	}

	/* Rotate and translate for initial view */
	rotate_y(PI);
	translate(0, 0, 5);

	return;
}

/* Render to screen buffer */
void render_to_buffer()
{
	unsigned int i, j;
	int x, y;

	/* Clear before start */
	int material_index = 0;
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

		/* Raster the vertex to screen */
		float x_array[3], y_array[3]; 
		
		x_array[0] = (tris_buffer[i*3+0].x / -absolute(tris_buffer[i*3+0].z) * buffer_width) + buffer_width/2;
		x_array[1] = (tris_buffer[i*3+1].x / -absolute(tris_buffer[i*3+1].z) * buffer_width) + buffer_width/2;
		x_array[2] = (tris_buffer[i*3+2].x / -absolute(tris_buffer[i*3+2].z) * buffer_width) + buffer_width/2;

		y_array[0] = (tris_buffer[i*3+0].y / -absolute(tris_buffer[i*3+0].z) * buffer_height)*screen_rateo + buffer_height/2; 
		y_array[1] = (tris_buffer[i*3+1].y / -absolute(tris_buffer[i*3+1].z) * buffer_height)*screen_rateo + buffer_height/2;
		y_array[2] = (tris_buffer[i*3+2].y / -absolute(tris_buffer[i*3+2].z) * buffer_height)*screen_rateo + buffer_height/2;

		for (j = 0; j < 3; j++)
		{
			if (x_array[j] < min_x)
				min_x = (int) x_array[j];
			if (x_array[j] > max_x)
				max_x = (int) x_array[j];
			if (y_array[j] < min_y)
				min_y = (int) y_array[j];
			if (y_array[j] > max_y)
				max_y = (int) y_array[j];
		}
		
		/* Check boundaries */
		max_x = (max_x > buffer_width-1) ? buffer_width-1 : max_x;
		max_y = (max_y > buffer_height-1) ? buffer_height-1 : max_y;
		min_x = (min_x < 0) ? 0 : min_x;
		min_y = (min_y < 0) ? 0 : min_y;
		
		/* Calculate the determinant for barycentric coordinate */
		determinant = 1/((y_array[1]-y_array[2])*(x_array[0]-x_array[2]) +
					(x_array[2]-x_array[1])*(y_array[0]-y_array[2]));

		/* Test only the pixel in this area */
		for (y = min_y; y <= max_y; y++) 
		{
			for (x = min_x; x <= max_x; x++) 
			{
				/* Calculate barycentric coordinate */
				float lambda0 = ((y_array[1]-y_array[2])*(x-x_array[2]) +
					(x_array[2]-x_array[1])*(y-y_array[2]))*determinant;
				float lambda1 = ((y_array[2]-y_array[0])*(x-x_array[2]) +
					(x_array[0]-x_array[2])*(y-y_array[2]))*determinant;
				float lambda2 = 1 - lambda0 - lambda1;
				
				/* If is inside the triangle, render it */
				if (lambda0 >= 0 && lambda1 >= 0 && lambda2 >= 0) 
				{
					/* Interpolate Z value */
					float pixel_depth = tris_buffer[i*3+0].z * lambda0 +
								tris_buffer[i*3+1].z * lambda1 +
								tris_buffer[i*3+2].z * lambda2;

					/* Test depth buffer and near plane */
					if (depth[x+y*buffer_width] > pixel_depth && pixel_depth > NEAR_PLANE)
					{
						/* Update both buffer */
						screen[x+y*buffer_width] = material_array[material_index];
						depth[x+y*buffer_width] = pixel_depth;
					}
				}
			}			
		}

		/* Render next face with another material */
		material_index++;

		/* Return to 0 if last element is reached */
		if (material_index == sizeof(material_array)/sizeof(char))
			material_index = 0;

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

	/* Render to buffer */
	render_to_buffer();
	
	/* Clear the console */
	clear_screen();

	#ifdef NCURSES
	/* Print it, Ncurses mode */
	for (j = 0; j < buffer_height; j++) 
	{
		for (i = 0; i < buffer_width; i++)
		{
			mvaddch(j, i, screen[i+j*buffer_width]);
		}
	}

	/* Print status info */
	printw("T(%.2f, %.2f, %.2f); R(%d, %d, %d); S(%.2f, %.2f, %.2f); V: %d; T: %d",
		(double) translation_x, (double) translation_y, (double) translation_z,
		(int)(rotation_x*180/PI), (int)(rotation_y*180/PI), (int)(rotation_z*180/PI),
		(double) scale_x, (double) scale_y, (double) scale_z, vertex_count, tris_count);

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
	
	/* Print status info */
	printf("T(%.2f, %.2f, %.2f); R(%d, %d, %d); S(%.2f, %.2f, %.2f)\n[V: %d; T: %d]> ",
		(double) translation_x, (double) translation_y, (double) translation_z,
		(int)(rotation_x*180/PI), (int)(rotation_y*180/PI), (int)(rotation_z*180/PI),
		(double) scale_x, (double) scale_y, (double) scale_z, vertex_count, tris_count);
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
	#ifdef NCURSES
	/* Reserve 1 line for status info, Ncurses mode */
	height -= 1;

	#else
	/* Reserve 2 line for status info, CLI mode */
	height -= 2;
	#endif
	
	/* Check width and height to be more than zero */
	if (width <= 0 || height <= 0)
		return;

	/* Free old buffer */
	free_buffer();

	/* Allocate new one */
	screen = (char*) malloc(sizeof(char) * (unsigned int)width * (unsigned int)height);
	depth = (float*) malloc(sizeof(float) * (unsigned int)width * (unsigned int)height);

	/* Set global buffer size */
	buffer_width = (int) width;
	buffer_height = (int) height;

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
	
		/* Quit */
		if (command == 'q')
			quit = 1;

		/* Help */
		else if (command == 'h')
			show_help();

		/* Move up */
		else if (command == 'w')
		{
			translation_y += 0.05f;
			translate(0, 0.05f, 0);
		}

		/* Move down */
		else if (command == 's')
		{
			translation_y -= 0.05f;
			translate(0, -0.05f, 0);
		}

		/* Move left */
		else if (command == 'a')
		{
			translation_x -= 0.05f;
			translate(-0.05f, 0, 0);
		}

		/* Move right */
		else if (command == 'd')
		{
			translation_x += 0.05f;
			translate(0.05f, 0, 0);
		}

		/* Move forward */
		else if (command == 'z')
		{
			translation_z -= 0.05f;
			translate(0, 0, -0.05f);
		}

		/* Move backward */
		else if (command == 'x')
		{
			translation_z += 0.05f;
			translate(0, 0, 0.05f);
		}

		/* Scale up */
		else if (command == '+')
		{
			scale_x *= 1.1f;
			scale_y *= 1.1f;
			scale_z *= 1.1f;
			translate(-translation_x, -translation_y, -translation_z);
			scale(1.1f, 1.1f, 1.1f);
			translate(translation_x, translation_y, translation_z);
		}

		/* Scale down */
		else if (command == '-')
		{
			scale_x *= 0.9f;
			scale_y *= 0.9f;
			scale_z *= 0.9f;
			translate(-translation_x, -translation_y, -translation_z);
			scale(0.9f, 0.9f, 0.9f);
			translate(translation_x, translation_y, translation_z);
		}

		/* Rotate y */
		else if (command == 'j')
		{	
			rotation_y = normalized_angle(rotation_y + 0.05f);
			translate(-translation_x, -translation_y, -translation_z);
			rotate_y(0.05f);
			translate(translation_x, translation_y, translation_z);
		}
		else if (command == 'l')
		{	
			rotation_y = normalized_angle(rotation_y - 0.05f);
			translate(-translation_x, -translation_y, -translation_z);
			rotate_y(-0.05f);
			translate(translation_x, translation_y, translation_z);
		}

		/* Rotate x */
		else if (command == 'i')
		{	
			rotation_x = normalized_angle(rotation_x + 0.05f);
			translate(-translation_x, -translation_y, -translation_z);
			rotate_x(0.05f);
			translate(translation_x, translation_y, translation_z);
		}
		else if (command == 'k')
		{	
			rotation_x = normalized_angle(rotation_x - 0.05f);
			translate(-translation_x, -translation_y, -translation_z);
			rotate_x(-0.05f);
			translate(translation_x, translation_y, translation_z);
		}

		/* Rotate z */
		else if (command == 'u')
		{	
			rotation_z = normalized_angle(rotation_z + 0.05f);
			translate(-translation_x, -translation_y, -translation_z);
			rotate_z(0.05f);
			translate(translation_x, translation_y, translation_z);
		}
		else if (command == 'o')
		{	
			rotation_z = normalized_angle(rotation_z - 0.05f);
			translate(-translation_x, -translation_y, -translation_z);
			rotate_z(-0.05f);
			translate(translation_x, translation_y, translation_z);
		}

		/* Restore mesh */
		else if (command == 'r')
			restore_mesh();
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
				translation_x += ammount;
			}
			else if (command[1] == 'y')
			{
				translate(0, ammount, 0);
				translation_y += ammount;
			}
			else if (command[1] == 'z')
			{
				translate(0, 0, ammount);
				translation_z += ammount;
			}
		}

		/* Rotation */
		else if (command[0] == 'r')
		{
			/* Convert from degree to radian */
			float angle = ammount*PI/180;
			
			/* Reposition the mesh to center */
			translate(-translation_x, -translation_y, -translation_z);
			
			/* Rotate */
			if (command[1] == 'x')
			{
				rotate_x(angle);
				rotation_x += angle;

				/* Display rotation between 0 and 360 */
				rotation_x = normalized_angle(rotation_x);
			}
			else if (command[1] == 'y')
			{
				rotate_y(angle);
				rotation_y += angle;

				/* Display rotation between 0 and 360 */
				rotation_y = normalized_angle(rotation_y);
			}
			else if (command[1] == 'z')
			{
				rotate_z(angle);
				rotation_z += angle;

				/* Display rotation between 0 and 360 */
				rotation_z = normalized_angle(rotation_z);
			}

			/* Reposition the mesh back */
			translate(translation_x, translation_y, translation_z);
		}

		/* Scale */
		else if (command[0] == 's')
		{
			/* Reposition the mesh to center */
			translate(-translation_x, -translation_y, -translation_z);	
			
			/* Scale */
			if (command[1] == 'x')
			{
				scale(ammount, 1, 1);
				scale_x *= ammount;
			}
			else if (command[1] == 'y')
			{
				scale(1, ammount, 1);
				scale_y *= ammount;
			}
			else if (command[1] == 'z')
			{
				scale(1, 1, ammount);
				scale_z *= ammount;
			}
			else if (command[1] == 'a')
			{
				scale(ammount, ammount, ammount);
				scale_x *= ammount;
				scale_y *= ammount;
				scale_z *= ammount;
			}

			/* Reposition the mesh back */
			translate(translation_x, translation_y, translation_z);
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
	free_tris();
	free_buffer();
	
	/* Kill the windows */
	#ifdef NCURSES
	endwin();
	#endif

	/* Exit */
	return 0;
}
