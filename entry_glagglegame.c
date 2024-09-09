Vector4 bg_box_col = {0, 0, 0, 0.5};
const int tile_width = 8;
const float entity_selection_radius = 16.0f;
const float player_pickup_radius = 20.0f;
const int rock_health = 2;
const int tree_health = 2;

typedef struct Range2f {
	Vector2 min;
	Vector2 max;
} Range2f;

inline Range2f range2f_make(Vector2 min, Vector2 max) { return (Range2f){ min, max }; }

Range2f range2f_shift(Range2f r, Vector2 shift){
	r.min = v2_add(r.min, shift);
	r.max = v2_add(r.max, shift);
	return r;
}

Range2f range2f_make_bottom_center(Vector2 size){
	Range2f range = {0};
	range.max = size;
	range = range2f_shift(range, v2(size.x * -0.5, 0.0));
	return range;
}

Vector2 range2f_size(Range2f range){
	Vector2 size = {0};
	size = v2_sub(range.min, range.max);
	size.x = fabsf(size.x);
	size.y = fabsf(size.y);
	return size;
}

bool range2f_contains(Range2f range, Vector2 v){
	return v.x >= range.min.x && v.x <= range.max.x && v.y >= range.min.y && v.y <= range.max.y;
}

Vector2 range2f_get_center(Range2f r) {
	return (Vector2) { (r.max.x - r.min.x) * 0.5 + r.min.x, (r.max.y - r.min.y) * 0.5 + r.min.y };
}
Draw_Quad ndc_quad_to_screen_quad(Draw_Quad ndc_quad) {

	// NOTE: we're assuming these are the screen space matricies.
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;

	Matrix4 ndc_to_screen_space = m4_identity();
	ndc_to_screen_space = m4_mul(ndc_to_screen_space, m4_inverse(proj));
	ndc_to_screen_space = m4_mul(ndc_to_screen_space, view);

	ndc_quad.bottom_left = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.bottom_left), 0, 1)).xy;
	ndc_quad.bottom_right = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.bottom_right), 0, 1)).xy;
	ndc_quad.top_left = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.top_left), 0, 1)).xy;
	ndc_quad.top_right = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.top_right), 0, 1)).xy;

	return ndc_quad;
}

Range2f quad_to_range(Draw_Quad quad) {
	return (Range2f){quad.bottom_left, quad.top_right};
}


inline float v2_dist(Vector2 a, Vector2 b){
	return v2_length(v2_sub(a, b));
}

float sin_breathe(float time, float rate){
	return sin((time * rate + 1) * 2);
}

int world_pos_to_tile_pos(float world_pos) {
	return roundf(world_pos / (float)tile_width);
}

float tile_pos_to_world_pos(int tile_pos) {
	return ((float)tile_pos * (float)tile_width);
}

Vector2 round_v2_to_tile(Vector2 world_pos) {
	world_pos.x = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.x));
	world_pos.y = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.y));
	return world_pos;
}

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
} Sprite;

typedef enum SpriteID{
	SPRITE_nil,
	SPRITE_tree0,
	SPRITE_tree1,
	SPRITE_rock0,
	SPRITE_player,
	SPRITE_item_rock,
	SPRITE_item_wood,
	SPRITE_MAX,
} SpriteID;
Sprite sprites[SPRITE_MAX];

Vector2 get_sprite_size(Sprite* sprite){
	return (Vector2){ sprite->image->width, sprite->image->height };
}

typedef enum ArchetypeID{
	ARCH_nil = 0,
	ARCH_tree = 1,
	ARCH_rock = 2,
	ARCH_player = 3,

	ARCH_item_rock = 4,
	ARCH_item_wood = 5,

	ARCH_MAX
}ArchetypeID;

typedef struct Entity {
	bool is_valid;
	ArchetypeID arch;
	Vector2 pos;
	SpriteID sprite_id;
	bool render_sprite;
	bool is_destructible;
	bool is_item;
	bool is_selectable;
	int health;
} Entity;

typedef struct ItemData {
	int amount;
	ArchetypeID type;
} ItemData;

typedef enum UXState {
	UX_nil,
	UX_inventory,
} UXState;

// :world
#define MAX_ENTITY_COUNT 1024
typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];
	ItemData inventory_items[ARCH_MAX];
	UXState ux_state;
	float inventory_alpha;
	float inventory_alpha_target;

} World;
World* world = 0;

typedef struct WorldFrame{
	Entity* selected_entity;
}WorldFrame;
WorldFrame world_frame;

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

string get_archetype_pretty_name(ArchetypeID arch){
	switch (arch)
	{
	case ARCH_item_rock: return STR("Rock"); break;
	case ARCH_item_wood: return STR("Wood"); break;
	default: return STR("Item not found"); break;
	}
}

void entity_destroy(Entity* entity){
	memset(entity, 0, sizeof(Entity));
}

void setup_tree(Entity* en){
	en->arch = ARCH_tree;
	if (get_random_float32_in_range(0, 1) < 0.5) {
		en->sprite_id = SPRITE_tree0;
	} else {
		en->sprite_id = SPRITE_tree1;
	}
	en->health = tree_health;
	en->is_destructible = true;
	en->is_selectable = true;
}
void setup_player(Entity* en){
	en->arch = ARCH_player;
	en->sprite_id = SPRITE_player;
	en->is_selectable = false;

}
void setup_rock(Entity* en){
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock0;
	en->health = rock_health;
	en->is_destructible = true;
	en->is_selectable = true;
}
void setup_item_rock(Entity* en){
	en->arch = ARCH_item_rock;
	en->sprite_id = SPRITE_item_rock;
	en->is_item = true;
	en->is_selectable = false;
}
void setup_item_wood(Entity* en){
	en->arch = ARCH_item_wood;
	en->sprite_id = SPRITE_item_wood;
	en->is_item = true;
	en->is_selectable = false;
}

Sprite* get_sprite(SpriteID id){
	if(id >= 0 && id < SPRITE_MAX){
		return &sprites[id];
	}
	return &sprites[0];
}

SpriteID get_sprite_id_from_arch(ArchetypeID arch){
	switch (arch)
	{
	case ARCH_item_rock: return SPRITE_item_rock; break;
	case ARCH_item_wood: return SPRITE_item_wood; break;
	default: return 0;
	}
}

Vector2 get_mouse_pos_in_ndc() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
	float window_w = window.width;
	float window_h = window.height;

	// Normalize the mouse coordinates
	float ndc_x = (mouse_x / (window_w * 0.5f)) - 1.0f;
	float ndc_y = (mouse_y / (window_h * 0.5f)) - 1.0f;

	return (Vector2){ ndc_x, ndc_y };
}

Vector2 screen_to_world() {
	float mouse_x = input_frame.mouse_x;
	float mouse_y = input_frame.mouse_y;
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
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
	window.width = 1280; 
	window.height = 720; 
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0xA9A9A9ff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	float64 last_time = os_get_elapsed_seconds();

	sprites[0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/nil.png"), get_heap_allocator()) };
	sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/player.png"), get_heap_allocator()) };
	sprites[SPRITE_tree0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree0.png"), get_heap_allocator()) };
	sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/tree1.png"), get_heap_allocator()) };
	sprites[SPRITE_rock0] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/rock0.png"), get_heap_allocator()) };
	sprites[SPRITE_item_rock] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_rock.png"), get_heap_allocator()) };
	sprites[SPRITE_item_wood] = (Sprite){ .image=load_image_from_disk(STR("res/sprites/item_wood.png"), get_heap_allocator()) };

	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());
	const u32 font_height = 48;

	// :init
	Entity* player_en = entity_create();
	setup_player(player_en);
	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_rock(en);
		en->pos = v2(get_random_float32_in_range(-100, 100), get_random_float32_in_range(-100, 100));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
	}

	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-100, 100), get_random_float32_in_range(-100, 100));
		en->pos = round_v2_to_tile(en->pos);
		en->pos.y -= tile_width * 0.5;
	}

	Vector2 camera_pos = v2(0, 0);

	while (!window.should_close) {
		float64 now = os_get_elapsed_seconds();
		// if ((int)now != (int)last_time) log("%.2f FPS\n%.2fms", 1.0/(now-last_time), (now-last_time)*1000);
		float64 delta_t = now - last_time;
		last_time = now;
		os_update(); 
		reset_temporary_storage();
		world_frame = (WorldFrame){0};

		// :camera
		{
			Vector2 target_pos = player_en->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 8.0f);
			float64 zoom = 5.3;
			draw_frame.camera_xform = m4_make_scale(v3(1.0, 1.0, 1.0));
			draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_translation(v3(camera_pos.x, camera_pos.y, 1.0)));
			draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}
		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);

		Vector2 mouse_pos_world = screen_to_world();
		// log("%f,  %f", mouse_pos_world.x, mouse_pos_world.y);

		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y);

		{
			// Vector2 pos = screen_to_world();
			// log("%f, %f", pos.x, pos.y);
			// draw_text(font, sprint(get_temporary_allocator(), STR("%f %f"), pos.x, pos.y), font_height, pos, v2(1, 1), COLOR_RED);
			float smallest_dist = INFINITY;

			for(int i = 0; i < MAX_ENTITY_COUNT; i++){
				Entity* en = &world->entities[i];
				if(en->is_valid && en->is_selectable){
					Sprite* sprite = get_sprite(en->sprite_id);
					int entity_tile_x = world_pos_to_tile_pos(en->pos.x);
					int entity_tile_y = world_pos_to_tile_pos(en->pos.y);
					float dist = fabsf(v2_dist(en->pos, mouse_pos_world));
					if(dist < entity_selection_radius){
						if(!world_frame.selected_entity || (dist < smallest_dist)){
							world_frame.selected_entity = en;
							smallest_dist = dist;
							
						}
					}
				}
			}
		}
		
		// :tile rendering
		{
			int player_tile_x = world_pos_to_tile_pos(player_en->pos.x);
			int player_tile_y = world_pos_to_tile_pos(player_en->pos.y);
			int tile_radius_x = 40;
			int tile_radius_y = 30;
			for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; x++) {
				for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; y++) {
					if ((x + (y % 2 == 0) ) % 2 == 0) {
						Vector4 col = v4(0.1, 0.1, 0.1, 0.1);
						float x_pos = x * tile_width;
						float y_pos = y * tile_width;
						draw_rect(v2(x_pos + tile_width * -0.5, y_pos + tile_width * -0.5), v2(tile_width, tile_width), col);
					}
				}
			}

			draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
		}
		
		// :update entities
		{
			for(int i = 0; i < MAX_ENTITY_COUNT; i++){
				Entity* en = &world->entities[i];
				if(en->is_valid && en->is_item){
					if(fabsf(v2_dist(en->pos, player_en->pos)) < player_pickup_radius){
						world->inventory_items[en->arch].amount += 1;
						entity_destroy(en);
					}
				}
			}
		}

		// :click destroy
		{
			Entity* selected_en = world_frame.selected_entity;
			if(is_key_just_pressed(MOUSE_BUTTON_LEFT) && selected_en->is_destructible && selected_en){
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);
				selected_en->health -= 1;
				if(selected_en->health <= 0){
					Entity* en = entity_create();
					switch (selected_en->arch)
					{
					case ARCH_rock:
						setup_item_rock(en);
						en->pos = selected_en->pos;
						break;
					case ARCH_tree:
						setup_item_wood(en);
						en->pos = selected_en->pos;
						break;
					default:
						break;
					}
					entity_destroy(selected_en);
				}
			}
		}

		// :render entities
		{
			for(int i = 0; i < MAX_ENTITY_COUNT; i++){
				Entity* en = &world->entities[i];
				if(en->is_valid){
					switch (en->arch)
					{
						default:
						{
							Sprite* sprite = get_sprite(en->sprite_id);
							
							Matrix4 xform = m4_scalar(1.0);
							if(en->is_item){
								xform = m4_translate(xform, v3(0, sin_breathe(os_get_elapsed_seconds(), 1.0), 0));
							}
							xform         = m4_translate(xform, v3(0, tile_width * -0.5, 0));
							xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
							xform         = m4_translate(xform, v3(sprite->image->width * -0.5, 0, 0));
							Vector4 col = COLOR_WHITE;
							if (world_frame.selected_entity == en) {
								col = COLOR_RED;
							}

							draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);

							// draw_text(font, sprint(get_temporary_allocator(), STR("%f %f"), en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_WHITE);
							break;
						}
					}
				}
			}
		}
		
		// :UI rendering
		
		{
			/*
			// TODO - cool inventory hovering above player and following
			Vector2 target_pos = player_en->pos;
			Vector2 inv_pos = v2(player_en->pos.x - 10, player_en->pos.y + 8);
			animate_v2_to_target(&inv_pos, camera_pos, delta_t, 1.0f);
			if(is_key_down('I')){
				
				draw_rect(inv_pos, v2(20, 15), v4(0.1, 0.1, 0.1, 0.5));
				for(int i = 0; i < ARCH_MAX; i++){
					ItemData* en = &world->inventory_items[i];
					if(en->amount > 0){
						draw_text(font, sprint(get_temporary_allocator(), STR("%d"), en->amount), font_height, v2_add(player_en->pos, v2((i * 5) - 20, 16)), v2(0.2, 0.2), COLOR_RED);
					}
				}
			}
			*/
			float width = 240.0;
			float height = 135.0;

			draw_frame.camera_xform = m4_scalar(1.0);
			draw_frame.projection = m4_make_orthographic_projection(0.0, width, 0.0, height, -1, 10);

			// :inventory UI
			if(is_key_just_pressed(KEY_TAB)){
				consume_key_just_pressed(KEY_TAB);
				world->ux_state = (world->ux_state == UX_inventory ? UX_nil : UX_inventory);
			}
			world->inventory_alpha_target = (world->ux_state == UX_inventory ? 1.0 : 0.0);
			animate_f32_to_target(&world->inventory_alpha, world->inventory_alpha_target, delta_t, 15.0);
			bool is_inventory_enabled = world->inventory_alpha_target == 1.0;
			if (world->inventory_alpha != 0.0)
			{

				float y_pos = 70.0;
				
				int item_count = 0;
				for(int i = 0; i < ARCH_MAX; i++){
					ItemData* item = &world->inventory_items[i];
					if(item->amount > 0){
						item_count++;
					}
				}

				const float icon_width = 8.0;
				float icon_object = icon_width;

				const int icon_count = 8;
				float hotbar_width = icon_object * icon_count;

				float x_start_pos = ((width - hotbar_width) / 2.0) + (icon_object * 0.5);

				// bg box
				{
					Matrix4 xform = m4_identity();
					xform = m4_translate(xform, v3(x_start_pos, y_pos, 0));
					draw_rect_xform(xform, v2(hotbar_width, icon_width), bg_box_col);
				}
				float slot_index = 0;
				for(int i = 0; i < ARCH_MAX; i++){
					ItemData* item = &world->inventory_items[i];
					if(item->amount > 0){
						float is_selected_alpha = 0.0;
						float slot_index_offset = slot_index * icon_object;

						Matrix4 xform = m4_identity();
						xform = m4_translate(xform, v3(x_start_pos + slot_index_offset, y_pos, 0.0));

						Sprite* sprite = get_sprite(get_sprite_id_from_arch(i));
						
						Draw_Quad* quad = draw_rect_xform(xform, v2(8, 8), v4(1, 1, 1, 0.2));
						Range2f icon_box = quad_to_range(*quad);
						if (is_inventory_enabled && range2f_contains(icon_box, get_mouse_pos_in_ndc())) {
							is_selected_alpha = 1.0;
						}

						Matrix4 box_bottom_right_xform = xform;
						xform = m4_translate(xform, v3(icon_width * 0.5, icon_width * 0.5, 0.0));
						
						// TODO - selection polish
						if(is_selected_alpha == 1.0)
						{
							float scale_adjust = 0.3; //0.1 * sin_breathe(os_get_elapsed_seconds(), 20.0);
							xform = m4_scale(xform, v3(1 + scale_adjust, 1 + scale_adjust, 1));
						}
						xform = m4_translate(xform, v3(get_sprite_size(sprite).x * -0.5, get_sprite_size(sprite).y * -0.5, 0.0));
						draw_image_xform(sprite->image, xform, get_sprite_size(sprite), COLOR_WHITE);
						// tooltip
						if(is_selected_alpha == 1.0)
						{
							Draw_Quad screen_quad = ndc_quad_to_screen_quad(*quad);
							Range2f screen_range = quad_to_range(screen_quad);
							Vector2 icon_center = range2f_get_center(screen_range);

							Matrix4 xform = m4_identity();

							Vector2 box_size = v2(40.0, 14.0);
							
							xform = m4_translate(xform, v3(box_size.x * -0.5, -box_size.y + (icon_width * -0.5), 0.0));
							xform = m4_translate(xform, v3(icon_center.x, icon_center.y, 0.0));
							draw_rect_xform(xform, box_size, bg_box_col);

							float current_y_pos = icon_center.y;
							{
								string title = get_archetype_pretty_name(i);
								Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));
								Vector2 draw_pos = icon_center;
								draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
								draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center

								draw_pos = v2_add(draw_pos, v2(0, icon_width * -0.5));
								draw_pos = v2_add(draw_pos, v2(0, -2.0)); // padding

								draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

								current_y_pos = draw_pos.y;
							}

							{
								string text = STR("x%i");
								text = sprint(get_temporary_allocator(), text, item->amount);

								Gfx_Text_Metrics metrics = measure_text(font, text, font_height, v2(0.1, 0.1));
								Vector2 draw_pos = v2(icon_center.x, current_y_pos);
								draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
								draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center

								draw_pos = v2_add(draw_pos, v2(0, -2.0)); // padding

								draw_text(font, text, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
							}
						}

						slot_index++;
					}
				}
			}

			
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

		

		gfx_update();
	}

	return 0;
}