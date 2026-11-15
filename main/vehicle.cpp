#include "vehicle.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace {
struct Point { float x, y; };
struct Vehicle { bool side; float angle; bool valid; bool rear; };
struct Painter {
    lv_draw_ctx_t *ctx;
    float cx, cy, scale, c, s;
    lv_opa_t opacity;
    lv_point_t transform(Point p) const {
        return {lv_coord_t(std::lround(cx + scale*(p.x*c-p.y*s))),
                lv_coord_t(std::lround(cy + scale*(p.x*s+p.y*c)))};
    }
    void line(Point a, Point b, uint32_t color=0xffb547, int width=1) const {
        auto p=transform(a),q=transform(b);
        lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);
        d.color=lv_color_hex(color);d.width=width;d.opa=opacity;
        d.round_start=d.round_end=1;
        lv_draw_line(ctx,&d,&p,&q);
    }
    void path(std::initializer_list<Point> points, int width=1) const {
        auto previous=points.begin();
        for(auto next=previous+1;next!=points.end();++next){line(*previous,*next,0xffb547,width);previous=next;}
    }
    void box(float x,float y,float w,float h,int width=1) const {
        path({{x,y},{x+w,y},{x+w,y+h},{x,y+h},{x,y}},width);
    }
    void circle(float x,float y,float radius,int width=1) const {
        constexpr int segments=32;
        Point old{x+radius,y};
        for(int i=1;i<=segments;i++){
            float a=i*6.2831853f/segments;Point next{x+radius*cosf(a),y+radius*sinf(a)};
            line(old,next,0xffb547,width);old=next;
        }
    }
};
void front(const Painter &p){
    // Upright windshield and hardtop, hood, flared fenders and seven-slot grille.
    p.path({{-34,-18},{-31,-43},{-27,-46},{27,-46},{31,-43},{34,-18}},2);
    p.path({{-27,-21},{-25,-39},{25,-39},{27,-21},{-27,-21}});
    p.line({-24,-24},{-9,-28});p.line({5,-24},{20,-28});
    p.path({{-36,-17},{-27,-20},{27,-20},{36,-17},{39,-9},{-39,-9},{-36,-17}},2);
    p.box(-39,-9,78,31,2);
    p.circle(-28,3,7,2);p.circle(28,3,7,2);
    for(int i=0;i<7;i++)p.box(-17+i*5,-3,2,18);
    p.path({{-40,-7},{-49,-6},{-51,4},{-40,4}},2);
    p.path({{40,-7},{49,-6},{51,4},{40,4}},2);
    p.box(-48,7,13,35,2);p.box(35,7,13,35,2);
    for(int y=10;y<40;y+=6){p.line({-48.f,float(y)},{-36.f,float(y+3)});p.line({36.f,float(y)},{48.f,float(y+3)});}
    p.path({{-44,23},{-44,31},{-22,31},{-17,27},{17,27},{22,31},{44,31},{44,23},{-44,23}},2);
    p.box(-34,25,9,3);p.box(25,25,9,3);
    p.line({-34,-16},{-44,-24});p.box(-51,-29,10,9);
    p.line({34,-16},{44,-24});p.box(41,-29,10,9);
    p.line({-15,34},{15,34});
    p.line({-56,47},{56,47},0xa3a6a3,2);
}
void rear(const Painter &p){
    // Hardtop rear window, tailgate, square lamps and rear-mounted spare.
    p.path({{-39,-12},{-35,-42},{-30,-46},{30,-46},{35,-42},{39,-12},{39,27},{-39,27},{-39,-12}},2);
    p.path({{-29,-18},{-27,-38},{27,-38},{29,-18},{-29,-18}});
    p.line({-16,-22},{12,-22});p.line({0,-24},{0,-20});
    p.box(-8,-44,16,3);
    p.path({{-39,-7},{-49,-5},{-51,5},{-39,5}},2);
    p.path({{39,-7},{49,-5},{51,5},{39,5}},2);
    p.box(-48,10,13,32,2);p.box(35,10,13,32,2);
    for(int y=13;y<40;y+=6){p.line({-48.f,float(y)},{-36.f,float(y+3)});p.line({36.f,float(y)},{48.f,float(y+3)});}
    p.box(-35,-6,10,16,2);p.box(25,-6,10,16,2);
    p.path({{-33,-4},{-27,8},{-27,-4},{-33,8}});
    p.path({{27,-4},{33,8},{33,-4},{27,8}});
    p.line({-24,-11},{-24,24});p.line({-24,24},{24,24});p.line({24,24},{24,13});
    p.circle(0,5,19,2);p.circle(0,5,13);p.circle(0,5,4,2);
    for(int i=0;i<12;i++){
        float a=i*6.2831853f/12;
        p.line({16*cosf(a),5+16*sinf(a)},{19*cosf(a+.09f),5+19*sinf(a+.09f)});
        if(i%2==0)p.line({5*cosf(a),5+5*sinf(a)},{12*cosf(a),5+12*sinf(a)});
    }
    p.box(-46,28,92,7,2);p.box(-32,29,15,4);p.box(-5,36,10,4);
    p.line({-56,47},{56,47},0xa3a6a3,2);
}
void wheel(const Painter &p,float x){
    p.circle(x,21,15,2);p.circle(x,21,10);p.circle(x,21,3);
    for(int i=0;i<8;i++){
        float a=i*6.2831853f/8;
        p.line({x+4*cosf(a),21+4*sinf(a)},{x+9*cosf(a),21+9*sinf(a)});
        p.line({x+13*cosf(a),21+13*sinf(a)},{x+15*cosf(a+.1f),21+15*sinf(a+.1f)});
    }
}
void side(const Painter &p){
    // Four doors, tall hardtop, short overhangs, external spare and round wheel arches.
    p.path({{-71,18},{-71,-3},{-61,-10},{-35,-13},{-19,-35},{48,-35},{57,-29},{63,-1},{65,17}},2);
    p.path({{-65,-7},{-34,-9},{-16,-30},{45,-30},{51,-26},{56,-5}});
    p.path({{-29,-11},{-15,-28},{-3,-28},{-3,-10},{-29,-11}});
    p.box(2,-28,20,18);p.path({{27,-28},{43,-28},{48,-23},{51,-10},{27,-10},{27,-28}});
    p.path({{-30,-6},{-26,18},{-3,18},{-3,-7}});
    p.path({{1,-7},{1,18},{25,18},{25,-7}});
    p.line({-25,23},{26,23},0xffb547,2);p.line({-23,27},{26,27});
    p.line({-11,-3},{-6,-3},0xffb547,2);p.line({17,-3},{22,-3},0xffb547,2);
    p.box(-31,-18,8,5);p.line({-28,-13},{-24,-8});
    p.path({{-70,17},{-65,17},{-60,3},{-38,3},{-31,17}},2);
    p.path({{26,17},{33,3},{52,3},{59,17},{66,17}},2);
    p.box(-77,13,9,7,2);p.box(61,14,10,6,2);p.box(-70,-1,5,5);
    p.box(63,-25,9,30,2);
    for(int y=-23;y<5;y+=5)p.line({64.f,float(y)},{71.f,float(y+2)});
    p.line({58,-12},{63,-12});p.box(56,-3,4,6);
    wheel(p,-48);wheel(p,43);
    p.line({-79,40},{79,40},0xa3a6a3,2);
}
void draw(lv_event_t *event){
    auto object=lv_event_get_target(event);
    auto state=static_cast<Vehicle*>(lv_event_get_user_data(event));
    if(lv_event_get_code(event)==LV_EVENT_DELETE){delete state;return;}
    lv_area_t area;lv_obj_get_coords(object,&area);
    const float view_angle=(!state->side && state->rear)?-state->angle:state->angle;
    float angle=std::clamp(view_angle,-45.f,45.f)*0.0174532925f;
    float c=cosf(angle),s=sinf(angle);
    float width=state->side?160:114,height=state->side?82:96;
    float available_w=lv_area_get_width(&area)-4,available_h=lv_area_get_height(&area)-4;
    float scale=std::min(available_w/(width*fabsf(c)+height*fabsf(s)),available_h/(width*fabsf(s)+height*fabsf(c)));
    scale=std::min(scale,state->side?1.0f:1.12f);
    Painter p{lv_event_get_draw_ctx(event),(area.x1+area.x2)*.5f,(area.y1+area.y2)*.5f,scale,c,s,lv_opa_t(state->valid?255:80)};
    if(state->side)side(p);else if(state->rear)rear(p);else front(p);
}
}
lv_obj_t *vehicle_create(lv_obj_t *parent,bool side_view,int x,int y,int width,int height){
    auto object=lv_obj_create(parent);lv_obj_remove_style_all(object);
    lv_obj_set_pos(object,x,y);lv_obj_set_size(object,width,height);
    lv_obj_clear_flag(object,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    auto state=new Vehicle{side_view,0,false,false};
    lv_obj_set_user_data(object,state);
    lv_obj_add_event_cb(object,draw,LV_EVENT_DRAW_MAIN,state);
    lv_obj_add_event_cb(object,draw,LV_EVENT_DELETE,state);
    return object;
}
void vehicle_set_attitude(lv_obj_t *object,float degrees,bool valid){
    auto state=static_cast<Vehicle*>(lv_obj_get_user_data(object));
    float next=valid?degrees:state->angle;
    if(fabsf(next-state->angle)<.15f && state->valid==valid)return;
    state->angle=next;state->valid=valid;lv_obj_invalidate(object);
}

void vehicle_set_rear(lv_obj_t *object,bool rear_view){
    auto state=static_cast<Vehicle*>(lv_obj_get_user_data(object));
    if(state->side || state->rear==rear_view)return;
    state->rear=rear_view;
    lv_obj_invalidate(object);
}
