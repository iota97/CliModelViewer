// CLI rasterization

// use tris only .obj model

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Help message
char *help_message =
	"CLI mesh rasterizer - Use with .obj model made only of tris	\n\
									\n\
	Syntax: mesh_viewer [path/to/mesh.obj]				\n\
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
				  'ry 1' rotate on y axis of 1 rad	\n\
				  'sa 0.5' scale all the axis by half	\n\
				  'm' reset mesh to start status	\n";

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
			tris_count++;
	}

	// Alloc the memory
	vertex_t *vertex_buffer = malloc(vertex_count * sizeof(vertex_t));
	tris_buffer = malloc(tris_count * sizeof(vertex_t) * 3);
	mesh_tris = malloc(tris_count * sizeof(vertex_t) * 3);

	// Reset the counter and rewind the file
	vertex_count = 0;
	tris_count = 0;
	rewind(mesh_file);

	// Cycle again, reading the data
	while(fgets(line_buffer, 1024, (FILE*) mesh_file)) 
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
			int a, b, c;
			sscanf(line_buffer, "%*s %u/%*s %u/%*s %u",
				 &a, &b, &c);

			// Bind the right vertex
			mesh_tris[tris_count*3+0] = vertex_buffer[a-1];
			mesh_tris[tris_count*3+1] = vertex_buffer[b-1];
			mesh_tris[tris_count*3+2] = vertex_buffer[c-1];
			
			// Increase the face count
			tris_count++;
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

// Rotate the mesh
void rotate(float x, float y, float z)
{
	for (int i = 0; i < tris_count; i++) 
	{
		for (int j = 0; j < 3; j++) 
		{
			// Back up float
			float tmp;
			
			// X
			tmp = tris_buffer[i*3+j].y;
			tris_buffer[i*3+j].y = cos(x) * tris_buffer[i*3+j].y -
						sin(x) * tris_buffer[i*3+j].z;
			tris_buffer[i*3+j].z = sin(x) * tmp +
						cos(x) * tris_buffer[i*3+j].z; 

			// Y
			tmp = tris_buffer[i*3+j].x;
			tris_buffer[i*3+j].x = cos(y) * tris_buffer[i*3+j].x -
						sin(y) * tris_buffer[i*3+j].z;
			tris_buffer[i*3+j].z = sin(y) * tmp +
						cos(y) * tris_buffer[i*3+j].z; 
			
			// Z
			tmp = tris_buffer[i*3+j].x;
			tris_buffer[i*3+j].x = cos(z) * tris_buffer[i*3+j].x -
						sin(z) * tris_buffer[i*3+j].y;
			tris_buffer[i*3+j].y = sin(z) * tmp +
						cos(z) * tris_buffer[i*3+j].y; 
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
	t_y = -1;
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
	translate(0, -1, -5);

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
		
		// Raster the vertex to screen
		double x_array[] = {
			(tris_buffer[i*3+0].x / tris_buffer[i*3+0].z * 80) + 80/2,
			(tris_buffer[i*3+1].x / tris_buffer[i*3+1].z * 80) + 80/2,
			(tris_buffer[i*3+2].x / tris_buffer[i*3+2].z * 80) + 80/2
		};
		double y_array[] = {
			(tris_buffer[i*3+0].y / tris_buffer[i*3+0].z * 40) + 40/2,
			(tris_buffer[i*3+1].y / tris_buffer[i*3+1].z * 40) + 40/2,
			(tris_buffer[i*3+2].y / tris_buffer[i*3+2].z * 40) + 40/2
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
		
		// Test only the pixel in this area
		for (int y = min_y; y < max_y; y++) 
		{
			for (int x = min_x; x < max_x; x++) 
			{
				int out = 0;
				
				for (int j = 0; j < 3; j++)
				{
					// Get the next vertex index
					int j1 = (j == 2) ? 0 : j + 1;	
					int j2 = (j1 == 2) ? 0 : j1 + 1;

					// Handle vertical line
					if (x_array[j] == x_array[j1])
					{	
						out += (x_array[j] < x_array[j2]) ? (x+1 < x_array[j]) : (x-1 > x_array[j]);
					}
					else 
					{
						// Handle horizzontal one
						if (y_array[j] == y_array[j1])
						{
							out += (y_array[j] < y_array[j2]) ? (y+1 < y_array[j]) : (y-1 > y_array[j]);
						}						
						else
						{ 
							// Check if the pixel is in the same direction of the other vertex
							if ((x_array[j2] - x_array[j])*(y_array[j1] - y_array[j]) < 
								(y_array[j2] - y_array[j])*(x_array[j1] - x_array[j]))
								
								// Then
								out += (x+1 - x_array[j])*(y_array[j1] - y_array[j]) > 
									(y+1 - y_array[j])*(x_array[j1] - x_array[j]);
							else 
								// Else
								out += (x-1 - x_array[j])*(y_array[j1] - y_array[j]) < 
									(y-1 - y_array[j])*(x_array[j1] - x_array[j]);
						}					
					}
				}

				// If is inside every line, render it
				if (!out) 
				{
					// Interpolate the depth
					float d = 0;
					for (int j = 0; j < 3; j++)
					{
						d += tris_buffer[i*3+j].z/sqrt((x-x_array[j])*(x-x_array[j]) +
							(y-y_array[j])*(y-y_array[j]));
					}
					
					// Test it
					if (depth[x][y] < d && d < -1)
					{
						// Update both buffer
						screen[x][y] = material_array[material_index];
						depth[x][y] = d;
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
	printf("T(%.2f, %.2f, %.2f); R(%.2f, %.2f, %.2f); S(%.2f, %.2f, %.2f)\n[V: %d; T: %d]> ",
		t_x, t_y, t_z, r_x, r_y, r_z, s_x, s_y, s_z, vertex_count, tris_count);

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
			
			// Rotate
			if (command[1] == 'x')
			{
				rotate(ammount, 0, 0);
				r_x += ammount;

				// Display rotation between -PI and PI
				if (r_x > M_PI)
					r_x -= M_PI + floor(r_x/(M_PI))*M_PI;
				else if (r_x < -M_PI)
					r_x += M_PI + floor(-r_x/(M_PI))*M_PI;
			}
			else if (command[1] == 'y')
			{
				rotate(0, ammount, 0);
				r_y += ammount;

				// Display rotation between -PI and PI
				if (r_y > M_PI)
					r_y -= M_PI + floor(r_y/(M_PI))*M_PI;
				else if (r_y < -M_PI)
					r_y += M_PI + floor(-r_y/(M_PI))*M_PI;
			}
			else if (command[1] == 'z')
			{
				rotate(0, 0, ammount);
				r_z += ammount;

				// Display rotation between -PI and PI
				if (r_z > M_PI)
					r_z -= M_PI + floor(r_z/(M_PI))*M_PI;
				else if (r_z < -M_PI)
					r_z += M_PI + floor(-r_z/(M_PI))*M_PI;
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

		// Reset command
		command[0] = 0;
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