#include "generation.h"
#include "template_picker.h"
#include "template_validation.h"
#include "templates.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using Json = nlohmann::json;
static void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static double distance(DoubleVector2 a, DoubleVector2 b) { return std::hypot(a.x-b.x,a.y-b.y); }
static bool inside(DoubleVector2 p, DoubleVector2 a, DoubleVector2 b, DoubleVector2 c) {
    const int x=planar::orientation(a,b,p),y=planar::orientation(b,c,p),z=planar::orientation(c,a,p);
    return (x>0&&y>0&&z>0)||(x<0&&y<0&&z<0);
}
struct Result { std::map<int, DoubleVector2> initial, positions; std::map<int,int> areas; std::vector<int> tiles; };
static Result generate(const Json& input, long long seed, int width=128, int height=128) {
    TemplatePicker picker; require(picker.tryReadEdgeGraph(input), "Cannot parse positional-magnet template");
    Generation g;g.isSeedRandom=false;g.seed=seed;g.generate(picker.mapTemplate,width,height);
    Result out;
    for(auto id:g.plane.getIDs())out.initial[id.getID()]=g.plane.getSpot(id).getCoords();
    g.runIteration([&](const Generation& state) {
        if(state.generationStage==GenerationStage::BORDERS)
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)++out.areas[state.grid->getTileID(x,y).getID()];
        return true;
    });
    require(g.is_done && g.plane.findEdgeIntersections().empty(), "Magnetic generation did not finish with a planar drawing");
    for(auto id:g.plane.getIDs())out.positions[id.getID()]=g.plane.getSpot(id).getCoords();
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        out.tiles.push_back(g.grid->getTileID(x,y).getID());out.tiles.push_back(int(g.spiceMap.get(x,y)));
    }
    return out;
}

static void checkRotation() {
    auto sameTargets = [](const PositionMagnetMap& a, const PositionMagnetMap& b) {
        if (a.size() != b.size()) return false;
        for (const auto& [id, target] : a) {
            const auto it = b.find(id);
            if (it == b.end() || distance(target.position, it->second.position) != 0 || target.force != it->second.force) return false;
        }
        return true;
    };
    for (auto [width, height] : {std::pair{128, 128}, std::pair{128, 80}, std::pair{32, 128}}) {
        for (bool corners : {false, true}) {
            MapTemplate map;
            map.positionMagnets = corners ? PositionMagnetMap{
                {Identifiable(1), {{0,0},2}}, {Identifiable(2), {{1,0},3}},
                {Identifiable(3), {{1,1},5}}, {Identifiable(4), {{0,1},7}}}
                : PositionMagnetMap{{Identifiable(1), {{.4,.45},2}}, {Identifiable(2), {{.5,.5},3}},
                    {Identifiable(3), {{.6,.55},5}}, {Identifiable(4), {{.45,.6},7}}};
            // Outside targets must not rotate back into the map and become active.
            map.positionMagnets.emplace(Identifiable(5), PositionMagnet{{1.1,.5},11});
            const auto original = map.positionMagnets;
            require(sameTargets(original, map.magnetsForGeneration(42,width,height)), "Disabled rotation changes magnet targets");
            map.rotatePositionMagnets = true;
            auto offset = [&](DoubleVector2 p) {
                return DoubleVector2{std::clamp(p.x*width,.5,width-.5)-width/2.0,
                    std::clamp(p.y*height,.5,height-.5)-height/2.0};
            };
            std::array<bool,4> quadrants{};
            bool continuous = false;
            const auto a = offset(original.at(1).position);
            for (std::uint64_t seed=0; seed<128; ++seed) {
                const auto rotated = map.magnetsForGeneration(seed,width,height);
                require(sameTargets(rotated,map.magnetsForGeneration(seed,width,height)), "Rotation is not seed-reproducible");
                require(sameTargets(original,map.positionMagnets), "Rotation mutates saved magnet targets");
                require(distance(rotated.at(5).position,original.at(5).position)==0, "Rotation activated an outside target");
                const auto b = offset(rotated.at(1).position);
                const double radiusSquared=a.x*a.x+a.y*a.y;
                const double cosine=(a.x*b.x+a.y*b.y)/radiusSquared, sine=(a.x*b.y-a.y*b.x)/radiusSquared;
                quadrants[(b.x<0?1:0)+(b.y<0?2:0)]=true;
                continuous |= std::abs(cosine)>.01 && std::abs(sine)>.01;
                for (int id : {1,2,3,4}) {
                    const auto before=offset(original.at(id).position), after=offset(rotated.at(id).position);
                    require(distance(after,{cosine*before.x-sine*before.y,sine*before.x+cosine*before.y})<1e-8,
                        "Magnet targets do not share one rigid rotation about the map center");
                    require(rotated.at(id).force==original.at(id).force, "Rotation changed magnet force");
                    const auto p=rotated.at(id).position;
                    require(p.x>=.5/width && p.x<=1-.5/width && p.y>=.5/height && p.y<=1-.5/height,
                        "A rotated target left the map's safe cell bounds");
                }
                if (corners) require(std::abs(sine)<1e-8 || (width==height && std::abs(cosine)<1e-8),
                    "Exact corners should allow only rigid quarter/half turns");
            }
            if (!corners) require(continuous && std::all_of(quadrants.begin(),quadrants.end(),[](bool value){return value;}),
                "Unrestricted magnets do not rotate continuously through all directions");
        }
    }
}

int main(int argc,char**argv) {
    try {
        checkRotation();
        require(argc>=2,"Pass Spice Scout fixture");Json base;std::ifstream(argv[1])>>base;
        auto magnetic=base;
        for(auto& node:magnetic["nodes"])if(node["id"]==5||node["id"]==15)
            node["data"]["magnet"]={{"force",5.0},{"position",{{"x",node["id"]==5?0.1:0.9},{"y",0.102778}}}};
        TemplatePicker picker;require(picker.tryReadEdgeGraph(magnetic)&&picker.mapTemplate->positionMagnets.size()==2,"Magnet targets were not loaded");
        Result first;
        for(long long seed:{2604084809LL,12419LL,1LL,42LL,2026LL}) {
            auto result=generate(magnetic,seed);
            std::cout<<"Seed "<<seed<<": areas 5="<<result.areas[5]<<", 15="<<result.areas[15]<<", 14="<<result.areas[14]<<'\n';
            require(!inside(result.initial[5],result.initial[10],result.initial[12],result.initial[16])
                && !inside(result.initial[15],result.initial[11],result.initial[13],result.initial[17]),
                "Magnets did not guide the initial choice of leaf faces");
            require(!inside(result.positions[5],result.positions[10],result.positions[12],result.positions[16]),"Zone 5 stayed trapped inside its neighboring triangle");
            require(!inside(result.positions[15],result.positions[11],result.positions[13],result.positions[17]),"Zone 15 entered its neighboring triangle");
            require(distance(result.positions[5],{12.8,128*.102778})<8&&distance(result.positions[15],{115.2,128*.102778})<8,"Corner magnets did not hold their target positions");
            require(result.areas[5]>250&&result.areas[15]>250,"A corner zone still has negligible area");
            if(seed==2604084809LL) {
                first=result;
                auto contact=[&](int a,int b){
                    for(int y=0;y<128;++y)for(int x=0;x<128;++x)for(auto [dx,dy]:{std::pair{1,0},std::pair{0,1}})
                        if(x+dx<128&&y+dy<128){int u=result.tiles[2*(y*128+x)],v=result.tiles[2*((y+dy)*128+x+dx)];if((u==a&&v==b)||(u==b&&v==a))return true;}
                    return false;
                };
                require(contact(5,16)&&contact(15,17),"The reference seed still has a blocked corner connection after walls");
            }
        }
        require(first.tiles==generate(magnetic,2604084809LL).tiles,"Position magnets are not deterministic");
        auto fixed=magnetic;fixed["magnets"]={{"randomRotation",false}};
        require(first.tiles==generate(fixed,2604084809LL).tiles,"Explicitly disabled rotation changed generation");
        auto rotating=magnetic;rotating["magnets"]={{"randomRotation",true}};
        require(picker.tryReadEdgeGraph(rotating)&&picker.mapTemplate->rotatePositionMagnets,"Global rotation setting was not loaded");
        Result firstRotated;
        for (long long seed : {2604084809LL,12419LL,42LL}) {
            const auto targets=picker.mapTemplate->magnetsForGeneration(seed,128,128);
            auto result=generate(rotating,seed);
            for (int id : {5,15}) {
                const auto target=targets.at(id).position;
                std::cout<<"Rotated seed "<<seed<<": zone "<<id<<" target "<<target.x<<','<<target.y
                    <<", error "<<distance(result.positions[id],{128*target.x,128*target.y})<<'\n';
                require(distance(result.positions[id],{128*target.x,128*target.y})<8,"Generation did not use rotated magnet targets");
                require(result.areas[id]>250,"Rotating magnets collapsed a corner zone");
            }
            if(seed==2604084809LL)firstRotated=result;
        }
        require(firstRotated.tiles!=first.tiles,"Enabled rotation did not change generation orientation");
        require(firstRotated.tiles==generate(rotating,2604084809LL).tiles,"Rotated map generation is not reproducible");
        auto noMagnets=base;noMagnets["magnets"]={{"randomRotation",true}};
        require(generate(noMagnets,42).tiles==generate(base,42).tiles,"Rotation without targets changed generation");
        auto centered=base;centered["nodes"][0]["data"]["magnet"]={{"force",5},{"position",{{"x",.5},{"y",.5}}}};
        auto rotatingCenter=centered;rotatingCenter["magnets"]={{"randomRotation",true}};
        require(generate(centered,42).tiles==generate(rotatingCenter,42).tiles,"Rotation perturbed a central target or consumed generation randomness");
        for (Json value : {Json(0),Json(1),Json("true"),Json(nullptr),Json::array(),Json::object()}) {
            auto bad=rotating;bad["magnets"]["randomRotation"]=value;
            require(!templateValidationError(bad).empty()&&!templateValidationError(bad,false).empty(),"Non-boolean rotation flag accepted");
        }
        for (Json value : {Json(false),Json(1),Json("bad"),Json(nullptr),Json::array()}) {
            auto bad=magnetic;bad["magnets"]=value;
            require(!templateValidationError(bad).empty(),"Invalid global magnet settings accepted");
        }
        require(first.tiles!=generate(base,2604084809LL).tiles,"Enabling positional magnets did not change generation");
        auto movedCanvas=magnetic;for(auto& n:movedCanvas["nodes"])n["visualizer"]={{"x",9999},{"y",-9999}};
        require(first.tiles==generate(movedCanvas,2604084809LL).tiles,"Visualizer coordinates leaked into generation");
        auto outside=base;outside["nodes"][0]["data"]["magnet"]={{"force",5},{"position",{{"x",1.1},{"y",.5}}}};
        require(generate(outside,12419).tiles==generate(base,12419).tiles,"An out-of-frame magnet affected generation");
        for(Json value:{Json(-1),Json(0),Json(101),Json("bad"),Json(nullptr)}) {
            auto bad=magnetic;for(auto& n:bad["nodes"])if(n["id"]==5)n["data"]["magnet"]["force"]=value;
            require(!templateValidationError(bad).empty(),"Invalid magnetic force accepted");
        }
        auto missing=magnetic;for(auto& n:missing["nodes"])if(n["id"]==5)n["data"]["magnet"].erase("position");
        require(!templateValidationError(missing).empty()&&templateValidationError(missing,false).empty(),"Missing generation target/draft handling is incorrect");
        // Isolate force strength and normalized coordinates from the central magnet.
        const auto graph=templates::makeGrid3x3(0);
        auto fit=[&](double force){
            EmbeddablePlane<BasicNode> p(128,80);
            p.initEmbed(graph,{},12419,nullptr,{{Identifiable(0),{{.15,.2},force}},{Identifiable(8),{{.85,.8},force}}});
            while(p.stepPlanar()){}
            require(p.findEdgeIntersections().empty(),"Target forces introduced a crossing");
            return distance(p.getSpot(0).getCoords(),{19.2,16})+distance(p.getSpot(8).getCoords(),{108.8,64});
        };
        require(fit(10)<fit(.1),"Increasing positional force did not improve target retention");
        std::cout<<"Positional targets, shared rotation, leaf face selection, force, validation, normalized bounds and repeatability passed\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
