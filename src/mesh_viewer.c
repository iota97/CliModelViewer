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
	Misc: 		R - reset	C - color			\n\
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

/* Vertex and tris counter */
static unsigned int tris_count = 0;
static unsigned int vertex_count = 0;

/* Tris Buffer */
static vertex_t *tris_buffer = NULL;

/* Screen and depth buffer */
static int buffer_width = 0;
static int buffer_height = 0;
static char *screen = NULL;
static float *depth = NULL;

/* Screen rateo based on screen height, width and font rateo */
static float screen_rateo;

/* Material array of char */
static char material_array[] = {'a', 'b', 'c', 'd', 
				'e', 'f', 'g', 'h',
				'i', 'j', 'k', 'l',
				'm', 'n', 'o', 'p'};

/* Transform matrix */
static float transform[4][4];

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
	free(tris_buffer);

	/* Alloc the memory */
	vertex_buffer = (vertex_t*) malloc(vertex_count * sizeof(vertex_t));
	tris_buffer = (vertex_t*) malloc(tris_count * sizeof(vertex_t) * 3);

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
			tris_buffer[tris_count*3+0] = vertex_buffer[vertex0-1];
			tris_buffer[tris_count*3+1] = vertex_buffer[vertex1-1];
			tris_buffer[tris_count*3+2] = vertex_buffer[vertex2-1];

			/* Increase tris count */
			tris_count++;
			
			/* Parse other tris if the face have more vertex */
			while (sscanf(line_buffer, "%u/", &vertex1) == 1)
			{
				/* Bind the vertex0, the new vertex and the last as a tris */
				tris_buffer[tris_count*3+0] = vertex_buffer[vertex0-1];
				tris_buffer[tris_count*3+1] = vertex_buffer[vertex1-1];
				tris_buffer[tris_count*3+2] = vertex_buffer[vertex2-1];

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
			depth[i+j*buffer_width] = FAR_PLANE;
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

	/* Translate for initial view */
	translate(0, 0, 5);

	return;
}

/* Render to screen buffer */
void render_to_buffer()
{
	unsigned int i, j;
	int x, y;

	/* Clear before start */
	unsigned int material_index = 0;
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

		/* Transformed vertex */
		vertex_t vertex[3];

		for (j = 0; j < 3; j++)
		{
			/* Multiply with transform matrix */
			vertex[j].x = transform[0][0]*tris_buffer[i*3+j].x+
					transform[1][0]*tris_buffer[i*3+j].y+
					transform[2][0]*tris_buffer[i*3+j].z+
					transform[3][0];

			vertex[j].y = transform[0][1]*tris_buffer[i*3+j].x+
					transform[1][1]*tris_buffer[i*3+j].y+
					transform[2][1]*tris_buffer[i*3+j].z+
					transform[3][1];

			vertex[j].z = transform[0][2]*tris_buffer[i*3+j].x+
					transform[1][2]*tris_buffer[i*3+j].y+
					transform[2][2]*tris_buffer[i*3+j].z+
					transform[3][2];

			/* Raster vertex to screen */
			x_array[j] = (vertex[j].x / -absolute(vertex[j].z) * buffer_width) + buffer_width/2;
			y_array[j] = (vertex[j].y / -absolute(vertex[j].z) * buffer_height)*screen_rateo + buffer_height/2;
		}

		/* Get boundaries */
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
					float pixel_depth = vertex[0].z * lambda0 +
								vertex[1].z * lambda1 +
								vertex[2].z * lambda2;

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
		if (material_index == sizeof(material_array)/sizeof(material_array[0]))
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
	/* Print it, Ncurses color mode */
	if (use_color)
	{
		for (j = 0; j < buffer_height; j++) 
		{
			for (i = 0; i < buffer_width; i++)
			{
				/* Set color attribute and print a blank space */
				attron(COLOR_PAIR(screen[i+j*buffer_width]));
				addch(' ');
			}
		}

		/* Restore black and white */
		attron(COLOR_PAIR(' '));
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
	printf("> ");
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
			init_pair(material_array[i], COLOR_WHITE, i%7+1);
		}

		/* Blank space */
		init_pair(' ', COLOR_WHITE, COLOR_BLACK);
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
	free(screen);
	free(depth);
	
	/* Kill the windows */
	#ifdef NCURSES
	endwin();
	#endif

	/* Exit */
	return 0;
}
