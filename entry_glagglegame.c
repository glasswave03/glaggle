
bool almost_equals(float a, float b, float epsilon) {
 return fabs(a - b) <= epsilon;
}

bool animate_f32_to_target(float* value, float target, float delta_t, float rate) {
	*value += (target - *value) * (1.0 - pow(2.0f, -rate * delta_t));
	if (almost_equals(*value, target, 0.001f))
	{
		*value = target;
		return true; // reached
	}
	return false;
}

void animate_v2_to_target(Vector2* value, Vector2 target, float delta_t, float rate) {
	animate_f32_to_target(&(value->x), target.x, delta_t, rate);
	animate_f32_to_target(&(value->y), target.y, delta_t, rate);
}

typedef struct Sprite {
	Gfx_Image* image;
	Vector2 size;
} Sprite;

typedef enum SpriteID{
	SPRITE_nil,
	SPRITE_tree0,
	SPRITE_tree1,
	SPRITE_rock0,
	SPRITE_player,
	SPRITE_MAX,
} SpriteID;
Sprite sprites[SPRITE_MAX];

typedef enum ArchetypeID{
	ARCH_nil = 0,
	ARCH_tree = 1,
	ARCH_rock = 2,
	ARCH_player = 3,

}ArchetypeID;

typedef struct Entity {
	bool is_valid;
	ArchetypeID arch;
	Vector2 pos;
	SpriteID sprite_id;
	bool render_sprite;
	
} Entity;

#define MAX_ENTITY_COUNT 1024
typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];

} World;
World* world = 0;


Entity* entity_create(){
	Entity* entity_found = 0;
	for(int i = 0; i < MAX_ENTITY_COUNT; i++){
		Entity* existing_entity = &world->entities[i];
		if(!existing_entity->is_valid){
			entity_found = existing_entity;
			break;
		}
	}
	assert(entity_found, "no more free entities");
	entity_found->is_valid = true;
	return entity_found;
}

void entity_destroy(Entity* entity){
	memset(entity, 0, sizeof(Entity));
}

void setup_tree(Entity* en){
	en->arch = ARCH_tree;
	en->sprite_id = SPRITE_tree0;
	// en->sprite_id = SPRITE_tree1;
}
void setup_player(Entity* en){
	en->arch = ARCH_player;
	en->sprite_id = SPRITE_player;

}
void setup_rock(Entity* en){
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock0;
}
Sprite* get_sprite(SpriteID id){
	if(id >= 0 && id < SPRITE_MAX){
		return &sprites[id];
	}
	return &sprites[0];
}

Vector2 screen_to_world() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.view;
	float window_w = window.width;
	float window_h = window.height;

	// Normalize the mouse coordinates
	float ndc_x = (mouse_x / (window_w * 0.5f)) - 1.0f;
	float ndc_y = (mouse_y / (window_h * 0.5f)) - 1.0f;

	// Transform to world coordinates
	Vector4 world_pos = v4(ndc_x, ndc_y, 0, 1);
	world_pos = m4_transform(m4_inverse(proj), world_pos);
	world_pos = m4_transform(view, world_pos);
	// log("%f, %f", world_pos.x, world_pos.y);

	// Return as 2D vector
	return (Vector2){ world_pos.x, world_pos.y };
}

int entry(int argc, char **argv) {
	
	window.title = STR("Glagglegame");
	window.scaled_width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.scaled_height = 720; 
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0xA9A9A9ff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	float64 last_time = os_get_current_time_in_seconds();

	sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("player.png"), get_heap_allocator()), .size=v2(8.0, 8.0)};
	sprites[SPRITE_tree0] = (Sprite){ .image=load_image_from_disk(STR("tree0.png"), get_heap_allocator()), .size=v2(8.0, 14.0)};
	sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("tree1.png"), get_heap_allocator()), .size=v2(11.0, 13.0)};
	sprites[SPRITE_rock0] = (Sprite){ .image=load_image_from_disk(STR("rock0.png"), get_heap_allocator()), .size=v2(8.0, 6.0)};

	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());
	const u32 font_height = 48;

	Entity* player_en = entity_create();
	setup_player(player_en);
	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_rock(en);
		en->pos = v2(get_random_float32_in_range(-100, 100), get_random_float32_in_range(-100, 100));
	}

	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-100, 100), get_random_float32_in_range(-100, 100));
	}

	Vector2 camera_pos = v2(0, 0);

	while (!window.should_close) {
		float64 now = os_get_current_time_in_seconds();
		if ((int)now != (int)last_time) log("%.2f FPS\n%.2fms", 1.0/(now-last_time), (now-last_time)*1000);
		float64 delta_t = now - last_time;
		last_time = now;
		os_update(); 
		reset_temporary_storage();

		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);

		// log("%f, %f", input_frame.mouse_x, input_frame.mouse_y);

		{
			Vector2 pos = screen_to_world();
			// log("%f, %f", pos.x, pos.y);
			draw_text(font, sprint(get_temporary_allocator(), STR("%f %f"), pos.x, pos.y), font_height, pos, v2(1, 1), COLOR_RED);
		}
		// :camera
		{
			Vector2 target_pos = player_en->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 8.0f);
			float64 zoom = 5.3;
			draw_frame.view = m4_make_scale(v3(1.0, 1.0, 1.0));
			draw_frame.view = m4_mul(draw_frame.view, m4_make_translation(v3(camera_pos.x, camera_pos.y, 1.0)));
			draw_frame.view = m4_mul(draw_frame.view, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}

		if(is_key_just_pressed(KEY_ESCAPE)){
			window.should_close = true;
		}

		Vector2 input_axis = v2(0, 0);
		if (is_key_down('A')) {
			input_axis.x -= 1.0;
		}
		if (is_key_down('D')) {
			input_axis.x += 1.0;
		}
		if (is_key_down('S')) {
			input_axis.y -= 1.0;
		}
		if (is_key_down('W')) {
			input_axis.y += 1.0;
		}
		input_axis = v2_normalize(input_axis);
		
		float64 movespeed = 100.0 * delta_t;
		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, movespeed));
		

		const int tile_width = 8;

		for (int x = 0; x < 10; x++) {
			for (int y = 0; y < 10; y++) {
				if ((x + (y % 2 == 0)) % 2 == 0) {
					Vector4 col = v4(0.1, 0.1, 0.1, 0.1);
					float x_pos = x * tile_width;
					float y_pos = y * tile_width;
					draw_rect(v2(x_pos, y_pos), v2(tile_width, tile_width), col);
				}
			}
		}

		for(int i = 0; i < MAX_ENTITY_COUNT; i++){
			Entity* en = &world->entities[i];
			if(en->is_valid){
				switch (en->arch)
				{
					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						
						Matrix4 xform = m4_scalar(1.0);
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(sprite->size.x *-0.5, 0, 0));
						draw_image_xform(sprite->image, xform, sprite->size, COLOR_WHITE);

						draw_text(font, sprint(get_temporary_allocator(), STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);
						break;
					}
				}
			}
		}


		gfx_update();
	}

	return 0;
}