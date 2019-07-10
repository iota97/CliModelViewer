// CLI rasterization

#include <stdio.h>
#include <stdlib.h>

// Help message
char help_message[] =
	"CLI mesh rasterizer - Use with .obj model format		\n\
									\n\
	In program syntax: [action][axis] [ammount]			\n\
									\n\
			action: t - translate				\n\
				r - rotate				\n\
				s - scale				\n\
				q - quit				\n\
				m - reset				\n\
				h - help				\n\
									\n\
			axis: x, y, z, a - all (scale only) 		\n\
									\n\
			ammount: float value				\n\
									\n\
			Examples: 'tx -0.2' translate on x axis by -0.2 \n\
				  'ry 90' rotate on y axis by 90 degree	\n\
				  'sa 0.5' scale all the axis by half	\n\
				  'm' reset mesh to start status	\n\
									\n\
			Press [Enter] to repeat the last command	\n";

// Math functions
#define PI 3.14159265359f

// Normalize rotation between 0 and 2PI
float normalized_angle(float x)
{
	if (x < 0)
		x += 2*PI*((int)(-x/(2*PI))+1);
	else 
		x -= 2*PI*(int)(x/(2*PI));

	return x;
}	

// Sine
float sine(float x)
{
	// Normalize the rotation
	x = normalized_angle(x);
		
	// Check sign
	float sign;
	if (x < PI)
		sign = 1;
	else
		sign = -1;

	// Check symmetry
	if (x < PI/2 || (x > PI && x < 3*PI/2))
		x -= (PI/2)*(int)(x/(PI/2));
	else
		x = PI/2 - x + (PI/2)*(int)(x/(PI/2));

	// Check it to be below 45 degree
	if (x < PI/4)
	{
		// Polinomial approximation
		float x2 = x*x;
		float x3 = x2*x;
		float sine = x - x3/6 + x2*x3/120;

		return sine*sign;
	}
	else
	{
		// Transform to cosine
		x = PI/2 - x;

		// Polinomial approximation
		float x2 = x*x;
		float x4 = x2*x2;
		float x6 = x2*x4;
		float cosine = 1 - x2/2 + x4/24 - x6/720;

		return cosine*sign;
	}
}

// Cosine
float cosine(float x)
{
	// Normalize the rotation
	x = normalized_angle(x);
		
	// Check sign
	float sign;
	if (x < PI/2 || x > 3*PI/2)
		sign = 1;
	else
		sign = -1;

	// Check symmetry
	if (x < PI/2 || (x > PI && x < 3*PI/2))
		x -= (PI/2)*(int)(x/(PI/2));
	else
		x = PI/2 - x + (PI/2)*(int)(x/(PI/2));

	// Check it to be below 45 degree
	if (x < PI/4)
	{
		// Polinomial approximation
		float x2 = x*x;
		float x4 = x2*x2;
		float x6 = x2*x4;
		float cosine = 1 - x2/2 + x4/24 - x6/720;

		return cosine*sign;
	}
	else
	{
		// Transform to sine
		x = PI/2 - x;

		// Polinomial approximation
		float x2 = x*x;
		float x3 = x2*x;
		float sine = x - x3/6 + x2*x3/120;

		return sine*sign;
	}
}

// Negative square root
float negsqrt(float x, float guess)
{
	guess *= -1;

	// Get closer with every iteration
	for (int i = 0; i < 6; i++)
	{
		guess = guess/2 + x/(2*guess);
	}

	return -guess;
}

// Vertex stuct
typedef struct vertex 
{
	float x;
	float y;
	float z;
} vertex_t;

// Translation
static float t_x = 0;
static float t_y = 0;
static float t_z = 0;

// Rotation
static float r_x = 0;
static float r_y = 0;
static float r_z = 0;

// Scale
static float s_x = 1;
static float s_y = 1;
static float s_z = 1;

// Vertex and tris counter
static int tris_count = 0;
static int vertex_count = 0;

// Tris Buffer, one for applying transform
static vertex_t *tris_buffer;
static vertex_t *mesh_tris;

// Screen and depth buffer
static char screen[80][40];
static float depth[80][40];

// Material array of char
static char material_array[] = {'a', 'b', 'c', 'd', 
				'e', 'f', 'g', 'h',
				'i', 'j', 'k', 'l',
				'm', 'n', 'o', 'p'};

// Read the mesh file
int parse_obj(char* path) 
{
	char line_buffer[1024];
	FILE* mesh_file = fopen(path, "r");

	// Check the read to be succefull
	if (mesh_file == NULL)
	{
		printf("Error reading file %s\n", path);
		return 1;
	}	

	// Cycle the file once to get the vertex and tris count
	while(fgets(line_buffer, 1024, mesh_file)) 
	{
		// Count vertex and tris ammount
		if (line_buffer[0] == 'v' && line_buffer[1] == ' ') 
			vertex_count++;
		else if (line_buffer[0] == 'f' && line_buffer[1] == ' ')
		{
			// Parse the vertex per face
			int a, b, c, d;
			int vertex_number = sscanf(line_buffer,
				"%*s %u/%*s %u/%*s %u/%*s %u",
				&a, &b, &c, &d);
			
			// If is a quad then count 2 tris
			if (vertex_number == 4)
				tris_count += 2;
			else
				tris_count += 1;
		}
	}

	// Alloc the memory
	vertex_t *vertex_buffer = (vertex_t*) malloc(vertex_count * sizeof(vertex_t));
	tris_buffer = (vertex_t*) malloc(tris_count * sizeof(vertex_t) * 3);
	mesh_tris = (vertex_t*) malloc(tris_count * sizeof(vertex_t) * 3);

	// Reset the counter and rewind the file
	vertex_count = 0;
	tris_count = 0;
	rewind(mesh_file);

	// Cycle again, reading the data
	while(fgets(line_buffer, 1024, mesh_file)) 
	{
		// Read vertex data
		if (line_buffer[0] == 'v' && line_buffer[1] == ' ') 
		{
			sscanf(line_buffer, "%*s %f %f %f", 
				&vertex_buffer[vertex_count].x, 
				&vertex_buffer[vertex_count].y, 
				&vertex_buffer[vertex_count].z);

			vertex_count++;
		}

		// Read face data
		if (line_buffer[0] == 'f' && line_buffer[1] == ' ') 
		{
			// Parse the vertex per face
			int a, b, c, d;
			int vertex_number = sscanf(line_buffer,
				"%*s %u/%*s %u/%*s %u/%*s %u",
				&a, &b, &c, &d);

			// Bind the right vertex
			mesh_tris[tris_count*3+0] = vertex_buffer[a-1];
			mesh_tris[tris_count*3+1] = vertex_buffer[b-1];
			mesh_tris[tris_count*3+2] = vertex_buffer[c-1];
			
			// Increase the face count
			tris_count++;

			// If is a quad then bind another tris
			if (vertex_number == 4)
			{
				mesh_tris[tris_count*3+0] = vertex_buffer[c-1];
				mesh_tris[tris_count*3+1] = vertex_buffer[d-1];
				mesh_tris[tris_count*3+2] = vertex_buffer[a-1];
				tris_count++;
			}
		}
	}

	// Free the vertex buffer
	free(vertex_buffer);

	// Close the file
	fclose(mesh_file);
	return 0;
}

// Translate the mesh
void translate(float x, float y, float z) 
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			tris_buffer[i*3+j].x += x; 
			tris_buffer[i*3+j].y += y;
			tris_buffer[i*3+j].z += z;
		}
	}

	return;
}

// Scale the mesh
void scale(float x, float y, float z)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			tris_buffer[i*3+j].x *= x; 
			tris_buffer[i*3+j].y *= y;
			tris_buffer[i*3+j].z *= z;
		}

	}

	return;
}

// Rotate the mesh on X axis
void rotate_x(float x)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			// Back up float
			float tmp = tris_buffer[i*3+j].y;
			
			tris_buffer[i*3+j].y = cosine(x) * tris_buffer[i*3+j].y -
						sine(x) * tris_buffer[i*3+j].z;
			tris_buffer[i*3+j].z = sine(x) * tmp +
						cosine(x) * tris_buffer[i*3+j].z;
		}
	}

	return;
}

// Rotate the mesh on Y axis
void rotate_y(float y)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			// Back up float
			float tmp = tris_buffer[i*3+j].x;
			
			tris_buffer[i*3+j].x = cosine(y) * tris_buffer[i*3+j].x -
						sine(y) * tris_buffer[i*3+j].z;
			tris_buffer[i*3+j].z = sine(y) * tmp +
						cosine(y) * tris_buffer[i*3+j].z; 
		}
	}

	return;
}

// Rotate the mesh on Z axis
void rotate_z(float z)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			// Back up float
			float tmp = tris_buffer[i*3+j].x;
			
			tris_buffer[i*3+j].x = cosine(z) * tris_buffer[i*3+j].x -
						sine(z) * tris_buffer[i*3+j].y;
			tris_buffer[i*3+j].y = sine(z) * tmp +
						cosine(z) * tris_buffer[i*3+j].y; 
		}
	}

	return;
}

// Clear the screen and depth buffer
void clear()
{
	for (int i = 0; i < 80; i++)
	{
		for (int j = 0; j < 40; j++)
		{
			screen[i][j] = ' ';
			depth[i][j] = -8192;
		}
	}

	return;
}

// Restore position
void restore()
{
	// Restore translation, scale and rotation
	t_x = 0;
	t_y = 0;
	t_z = -5;
	r_x = 0;
	r_y = 0;
	r_z = 0;
	s_x = 1;
	s_y = 1;
	s_z = 1;

	// Restore the tris from the mesh one
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			tris_buffer[i*3+j].x = mesh_tris[i*3+j].x; 
			tris_buffer[i*3+j].y = mesh_tris[i*3+j].y;
			tris_buffer[i*3+j].z = mesh_tris[i*3+j].z;
		}
	}

	// Translate for initial view
	translate(0, 0, -5);

	return;
}

// Render to screen buffer
void render()
{
	// Clear before start
	clear();
	int material_index = 0;

	for (int i = 0; i < tris_count; i++) 
	{
		// Calculate the depth of the vertex
		float depth_array[] = {
			negsqrt(tris_buffer[i*3+0].x*tris_buffer[i*3+0].x +
				tris_buffer[i*3+0].y*tris_buffer[i*3+0].y +
				tris_buffer[i*3+0].z*tris_buffer[i*3+0].z,
				tris_buffer[i*3+0].z),

			negsqrt(tris_buffer[i*3+1].x*tris_buffer[i*3+1].x +
				tris_buffer[i*3+1].y*tris_buffer[i*3+1].y +
				tris_buffer[i*3+1].z*tris_buffer[i*3+1].z,
				tris_buffer[i*3+0].z),

			negsqrt(tris_buffer[i*3+2].x*tris_buffer[i*3+2].x +
				tris_buffer[i*3+2].y*tris_buffer[i*3+2].y +
				tris_buffer[i*3+2].z*tris_buffer[i*3+2].z,
				tris_buffer[i*3+0].z),
		};
		
		// Raster the vertex to screen
		float x_array[] = {
			(tris_buffer[i*3+0].x / depth_array[0] * 80) + 80/2,
			(tris_buffer[i*3+1].x / depth_array[1] * 80) + 80/2,
			(tris_buffer[i*3+2].x / depth_array[2] * 80) + 80/2
		};
		float y_array[] = {
			(tris_buffer[i*3+0].y / depth_array[0] * 40) + 40/2,
			(tris_buffer[i*3+1].y / depth_array[1] * 40) + 40/2,
			(tris_buffer[i*3+2].y / depth_array[2] * 40) + 40/2
		};

		// Get the bounding coordinate of the tris
		int min_x = 80;
		int min_y = 40;
		int max_x = 0;
		int max_y = 0;

		for (int j = 0; j < 3; j++)
		{
			if (x_array[j] < min_x)
				min_x = x_array[j];
			if (x_array[j] > max_x)
				max_x = x_array[j];
			if (y_array[j] < min_y)
				min_y = y_array[j];
			if (y_array[j] > max_y)
				max_y = y_array[j];
		}
		
		// Check boundaries
		max_x = (max_x > 80) ? 80 : max_x;
		max_y = (max_y > 40) ? 40 : max_y;
		min_x = (min_x < 0) ? 0 : min_x;
		min_y = (min_y < 0) ? 0 : min_y;
		
		// Calculate the determinant for barycentric coordinate
		float determinant = 1/((y_array[1]-y_array[2])*(x_array[0]-x_array[2]) +
					(x_array[2]-x_array[1])*(y_array[0]-y_array[2]));

		// Test only the pixel in this area
		for (int y = min_y; y < max_y; y++) 
		{
			for (int x = min_x; x < max_x; x++) 
			{
				// Calculate barycentric coordinate
				float lambda0 = ((y_array[1]-y_array[2])*(x+1-x_array[2]) +
					(x_array[2]-x_array[1])*(y+1-y_array[2]))*determinant;
				float lambda1 = ((y_array[2]-y_array[0])*(x+1-x_array[2]) +
					(x_array[0]-x_array[2])*(y+1-y_array[2]))*determinant;
				float lambda2 = 1 - lambda0 - lambda1;
				
				// If is inside the triangle, render it
				if (lambda0 >= 0 && lambda1 >= 0 && lambda2 >= 0) 
				{
					// Interpolate the pixel depth
					float pixel_depth = depth_array[0] * lambda0 +
							depth_array[1] * lambda1 +
							depth_array[2] * lambda2;

					// Calculate an approximative signed depth
					float approx_signed_depth = tris_buffer[i*3+0].z * lambda0 +
								tris_buffer[i*3+1].z * lambda1 +
								tris_buffer[i*3+2].z * lambda2;

					// Test it
					if (depth[x][y] < pixel_depth && approx_signed_depth < -1)
					{
						// Update both buffer
						screen[x][y] = material_array[material_index];
						depth[x][y] = pixel_depth;
					}
				}
			}			
		}

		// Render next face with another material
		material_index++;

		// Return to 0 if last element is reached
		if (material_index == sizeof(material_array)/sizeof(char))
			material_index = 0;

	}

	return;
}

// Clear the console
void clear_screen()
{	
	// Print 172 new line
	for (int i = 0; i < 172; i++)
		printf("\n");

	return;
}

// Draw in the console
void draw()
{
	// Render to buffer
	render();
	
	// Clear the console
	clear_screen();

	// Print it
	for (int j = 0; j < 40; j++) 
	{
		for (int i = 0; i < 80; i++) 
		{
			printf("%c", screen[i][j]);
		}
		printf("\n");
	}
	
	// Print status info
	printf("T(%.2f, %.2f, %.2f); R(%d, %d, %d); S(%.2f, %.2f, %.2f)\n[V: %d; T: %d]> ",
		t_x, t_y, t_z, (int)(r_x*180/PI), (int)(r_y*180/PI), (int)(r_z*180/PI),
		s_x, s_y, s_z, vertex_count, tris_count);

	return;
}

// Help message

void help()
{
	// Clear the console
	clear_screen();

	// Print usage instruction
	printf("%s\nPress ENTER to continue ", help_message);
	
	// Wait input to proceed
	getchar();
	return;
}

// Input loop
void loop ()
{
	// Input variables
	char command[64];
	char last[2] = "\0";
	float ammount = 0;
	int quit = 0;

	// Keep looping until user hit 'q'
	for(;;)
	{	
		// Render it
		draw();

		// Get input
		fgets(command, 64, stdin);
		sscanf(command, "%*s %f", &ammount);
		
		// If just enter, restore the previous one
		if (command[0] == '\n')
		{
			command[0] = last[0];
			command[1] = last[1];
		}

		// Quit
		if (command[0] == 'q')
			break;

		// Translation
		else if (command[0] == 't')
		{
			// Translate
			if (command[1] == 'x')
			{
				translate(ammount, 0, 0);
				t_x += ammount;
			}
			else if (command[1] == 'y')
			{
				translate(0, ammount, 0);
				t_y += ammount;
			}
			else if (command[1] == 'z')
			{
				translate(0, 0, ammount);
				t_z += ammount;
			}
		}

		// Rotation
		else if (command[0] == 'r')
		{
			// Reposition the mesh to center
			translate(-t_x, -t_y, -t_z);

			// Convert from degree to radian
			float angle = ammount*PI/180;
			
			// Rotate
			if (command[1] == 'x')
			{
				rotate_x(angle);
				r_x += angle;

				// Display rotation between 0 and 360
				r_x = normalized_angle(r_x);
			}
			else if (command[1] == 'y')
			{
				rotate_y(angle);
				r_y += angle;

				// Display rotation between 0 and 360
				r_y = normalized_angle(r_y);
			}
			else if (command[1] == 'z')
			{
				rotate_z(angle);
				r_z += angle;

				// Display rotation between 0 and 360
				r_z = normalized_angle(r_z);
			}

			// Reposition the mesh back
			translate(t_x, t_y, t_z);
		}

		// Scale
		else if (command[0] == 's')
		{
			// Reposition the mesh to center
			translate(-t_x, -t_y, -t_z);	
			
			// Scale
			if (command[1] == 'x')
			{
				scale(ammount, 1, 1);
				s_x *= ammount;
			}
			else if (command[1] == 'y')
			{
				scale(1, ammount, 1);
				s_y *= ammount;
			}
			else if (command[1] == 'z')
			{
				scale(1, 1, ammount);
				s_z *= ammount;
			}
			else if (command[1] == 'a')
			{
				scale(ammount, ammount, ammount);
				s_x *= ammount;
				s_y *= ammount;
				s_z *= ammount;
			}

			// Reposition the mesh back
			translate(t_x, t_y, t_z);
		}

		// Reset
		else if (command[0] == 'm')
			restore();

		// Print help
		else if (command[0] == 'h')
			help();	

		// Save last command
		last[0] = command[0];
		last[1] = command[1];
	}

	return;
}

// Main
int main(int argc, char *argv[]) 
{

	// Check argument number
	if (argc < 2) 
	{		
		printf("Please provide the model path.\n");
		return 1;	
	}

	// Parse the model
	if (parse_obj(argv[1]))
		return 2;
	
	// Print help message at start
	help();	

	// Restore the mesh
	restore();

	// Start the loop
	loop();
	
	// Free memory
	free(mesh_tris);
	free(tris_buffer);

	// Exit
	return 0;
}