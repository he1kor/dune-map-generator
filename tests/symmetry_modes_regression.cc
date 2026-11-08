#include "generation.h"
#include "template_editor.h"
#include "template_picker.h"
#include "template_validation.h"
#include "planar_symmetry.h"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <thread>

using Json=nlohmann::json;
static void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
static bool inside(DoubleVector2 p,DoubleVector2 a,DoubleVector2 b,DoubleVector2 c){
    int x=planar::orientation(a,b,p),y=planar::orientation(b,c,p),z=planar::orientation(c,a,p);
    return(x>0&&y>0&&z>0)||(x<0&&y<0&&z<0);
}
struct Result { std::vector<int> tiles; double drift=0; int axis=0; };

static int completeAxis(const Generation& g,int width,int height) {
    const auto normal=g.plane.symmetryAxisNormal().value();
    if(std::abs(normal.y)<1e-6)return 0;
    if(std::abs(normal.x)<1e-6)return 1;
    require(width==height&&std::abs(std::abs(normal.x)-std::abs(normal.y))<1e-6,
        "Complete symmetry used an axis that does not preserve the tile grid");
    return normal.x*normal.y<0?2:3;
}
static void checkComplete(const Generation& g,const std::map<int,int>& partners,int width,int height) {
    const int axis=completeAxis(g,width,height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        // Independent formulas for all four grid reflections.
        const int mx=axis==0?width-1-x:axis==1?x:axis==2?y:width-1-y;
        const int my=axis==0?y:axis==1?height-1-y:axis==2?x:height-1-x;
        const int id=g.grid->getTileID(x,y).getID();
        // Diagonal cells reflect onto themselves and have one ownership label.
        // Elsewhere, the zone IDs must exchange with their partners exactly.
        if(mx!=x||my!=y)require(g.grid->getTileID(mx,my).getID()==partners.at(id),"Final zone ownership is not mirrored");
        require(g.spiceMap.get(mx,my)==g.spiceMap.get(x,y),"Final resource tiles are not mirrored");
        require(g.wallMask.get(mx,my)==g.wallMask.get(x,y),"Walls or passage openings are not mirrored");
        for(auto* masks:{&g.zoneMasks.influenceMask,&g.zoneMasks.bonusMask})
            for(const auto& [zone,mask]:*masks)
                if(mx!=x||my!=y||partners.at(zone.getID())==zone.getID())
                    require(mask.get(x,y)==masks->at(partners.at(zone.getID())).get(mx,my),"Blending masks are not mirrored");
    }
}

// Check every expansion/cleanup wave, not only the final projection. Paired
// zones may touch the axis but cannot form fingers across the opposite half.
static void runWithSeamChecks(Generation& g,const std::map<int,int>& partners) {
    std::map<int,bool> side;
    std::optional<Grid<ResourceRadialNode<Resource>>> beforeWalls;
    int checkedWaves=0;
    g.runIteration([&](const Generation& state) {
        if(!state.grid)return true;
        const int w=state.grid->getWidth(),h=state.grid->getHeight(),axis=completeAxis(state,w,h);
        auto canonical=[&](int x,int y) {
            const int mx=axis==0?w-1-x:axis==1?x:axis==2?y:w-1-y;
            const int my=axis==0?y:axis==1?h-1-y:axis==2?x:h-1-x;
            return y<my||(y==my&&x<=mx);
        };
        if(side.empty()) for(auto [id,partner]:partners)if(id>=0&&id!=partner) {
            const auto p=state.plane.getSpot(id).getCoords();
            side[id]=canonical(int(std::floor(p.x)),int(std::floor(p.y)));
        }
        if(state.generationStage==GenerationStage::ZONE_BLOAT||state.generationStage==GenerationStage::POST_PROCESS||state.generationStage==GenerationStage::BORDERS) {
            ++checkedWaves;
            for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                const int id=state.grid->getTileID(x,y).getID();
                if(side.contains(id))require(side.at(id)==canonical(x,y),"A paired zone grew across the symmetry axis");
            }
        }
        if(state.generationStage==GenerationStage::BORDERS)beforeWalls=*state.grid;
        if(state.generationStage==GenerationStage::NOISE) {
            require(beforeWalls.has_value(),"Missing pre-wall ownership snapshot");
            for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(state.wallMask.get(x,y)) {
                const auto owner=beforeWalls->getTileID(x,y);
                bool border=owner==Identifiable::nullID;
                for(auto d:{IntVector2{-1,0},{1,0},{0,-1},{0,1}}) {
                    const IntVector2 p{x+d.x,y+d.y};
                    if(beforeWalls->isValidPoint(p)&&beforeWalls->getTileID(p)!=owner)border=true;
                }
                require(border,"A cliff appeared inside a zone instead of on an actual border");
            }
            for(auto [id,partner]:partners)if(id>=0&&id==partner) {
                bool halves[2]={false,false};
                for(int y=0;y<h;++y)for(int x=0;x<w;++x)
                    if(beforeWalls->getTileID(x,y).getID()==id)halves[canonical(x,y)]=true;
                require(halves[0]&&halves[1],"An axis zone cannot span both halves");
            }
        }
        return true;
    });
    require(checkedWaves>2,"Growth/cleanup seam checks were not exercised");
}

static void resourceSeamCases(Json duel) {
    // No configured blending: every non-wall tile belongs fully to one zone.
    // Give all zones identical, deliberately non-enum-ordered intervals
    // so accidental blending cannot silently reorder their resources.
    duel["embedding"]={{"symmetry","complete"}};
    duel["asymmetrical_edges"]=Json::array();
    for(auto& node:duel["nodes"]) {
        node["data"]["resources"]={{"octave1_weight",1.0},{"octave2_weight",1.0},{"octave3_weight",1.0},
            {"content",Json::array({{{"name","sand"},{"startThreshold",0.0},{"endThreshold",.5}},
                                    {{"name","dunes"},{"startThreshold",.5},{"endThreshold",1.0}}})}};
    }
    for(auto& edge:duel["symmetrical_edges"])edge["data"]={
        {"passes",{{"minPassWidth",0},{"maxPassWidth",1000},{"minWallLength",0},{"maxWallLength",0}}},
        {"resourceBlend",{{"distance",0}}}};
    TemplatePicker picker;require(picker.tryReadEdgeGraph(duel),templateValidationError(duel));
    std::map<int,int> partners{{-1,-1}};const auto t=picker.mapTemplate->embeddingTopology;
    for(size_t i=0;i<t->ids.size();++i)partners[t->ids[i].getID()]=t->ids[picker.mapTemplate->embeddingReflection->partner[i]].getID();
    int seamMaterials=0;
    for(int seed:{3,10,13,21}) {
        Generation g;g.seed=seed;g.isSeedRandom=false;g.generate(picker.mapTemplate,128,128);
        runWithSeamChecks(g,partners);checkComplete(g,partners,128,128);
        const int axis=completeAxis(g,128,128);
        for(int y=0;y<128;++y)for(int x=0;x<128;++x) {
            const auto id=g.grid->getTileID(x,y);
            if(id==Identifiable::nullID) {
                require(g.wallMask.get(x,y),"Zone cleanup left an empty tile at the symmetry seam");
                continue; // Non-neighbor contacts still have walls.
            }
            for(const auto& [zone,mask]:g.zoneMasks.influenceMask)
                require(mask.get(x,y)==(zone==id?1.0:0.0),"Unconfigured blending was added at the symmetry seam");
            const double value=g.resourceGenerators.at(id).getNoise().get(x,y);
            const auto expected=value<.5?Resource::SAND:Resource::DUNES;
            require(g.spiceMap.get(x,y)==expected,"Symmetry seam changed configured resource interval ordering");
            const bool onDiagonal=(axis==2&&x==y)||(axis==3&&x+y==127);
            if(onDiagonal)seamMaterials|=value<.5?1:2;
        }
        for(auto [id,partner]:partners)if(id>=0&&id<partner)
            for(int y=0;y<128;++y)for(int x=0;x<128;++x)
                require(g.resourceGenerators.at(id).getNoise().get(x,y)==g.resourceGenerators.at(partner).getNoise().get(x,y),
                    "Paired zones use different percentile domains");
    }
    require(seamMaterials==3,"Diagonal resources remain pinned to one end of the thresholds");
}

static void diagonalCases() {
    Json duel;std::ifstream(std::filesystem::path(__FILE__).parent_path()/"fixtures/duel.json")>>duel;
    duel["embedding"]={{"symmetry","complete"}};
    TemplatePicker picker;require(picker.tryReadEdgeGraph(duel),templateValidationError(duel));
    std::map<int,int> partners{{-1,-1}};const auto t=picker.mapTemplate->embeddingTopology;
    for(size_t i=0;i<t->ids.size();++i)partners[t->ids[i].getID()]=t->ids[picker.mapTemplate->embeddingReflection->partner[i]].getID();
    std::set<int> seen;
    for(int seed=0;seed<64&&seen.size()<4;++seed){
        Generation g;g.seed=seed;g.isSeedRandom=false;g.generate(picker.mapTemplate,128,128);
        const int axis=completeAxis(g,128,128);
        if(!seen.insert(axis).second)continue;
        runWithSeamChecks(g,partners);require(g.is_done,"Duel complete generation did not finish");checkComplete(g,partners,128,128);
        std::cout<<"Duel Complete symmetry: axis "<<axis<<", seed "<<seed<<" passed\n";
    }
    require(seen.size()==4,"Seeds never selected both diagonals and both horizontal/vertical axes");
    resourceSeamCases(duel);
    for(int axis:{2,3}){
        auto fixed=duel;
        for(auto& node:fixed["nodes"])if(node["id"]==4||node["id"]==6){
            const double v=node["id"]==4?.15:.6;
            node["data"]["magnet"]={{"force",10},{"position",{{"x",v},{"y",axis==2?v:1-v}}}};
        }
        TemplatePicker magnets;require(magnets.tryReadEdgeGraph(fixed),templateValidationError(fixed));
        Generation g;g.seed=42;g.isSeedRandom=false;g.generate(magnets.mapTemplate,64,64);
        require(completeAxis(g,64,64)==axis,"A fixed diagonal magnet axis was rejected or changed");
        g.runIteration();checkComplete(g,partners,64,64);
        Generation background;background.seed=42;background.isSeedRandom=false;
        require(background.startAsync(magnets.mapTemplate,64,64),"Cannot start diagonal async generation");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        while(background.isRunning()&&std::chrono::steady_clock::now()<deadline){background.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(!background.isRunning()&&background.generationError.empty()&&background.is_done,"Diagonal async generation failed");
        checkComplete(background,partners,64,64);
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)
            require(g.spiceMap.get(x,y)==background.spiceMap.get(x,y),"Diagonal async output differs from synchronous output");
        bool rejected=false;
        try{magnets.mapTemplate->reflectionForGeneration(magnets.mapTemplate->magnetsForGeneration(42,128,80),128,80);}
        catch(const planar::ConflictingSymmetryMagnets&){rejected=true;}
        require(rejected,"A non-square map accepted diagonal tile reflection");
        fixed["magnets"]={{"randomRotation",true}};
        TemplatePicker rotating;require(rotating.tryReadEdgeGraph(fixed),templateValidationError(fixed));
        std::set<int> rotatedAxes;
        for(int seed=0;seed<32&&rotatedAxes.size()<4;++seed){
            Generation rotated;rotated.seed=seed;rotated.isSeedRandom=false;rotated.generate(rotating.mapTemplate,128,128);
            const int a=completeAxis(rotated,128,128);
            if(!rotatedAxes.insert(a).second)continue;
            rotated.runIteration();checkComplete(rotated,partners,128,128);
        }
        require(rotatedAxes.size()==4,"Random magnet rotation omitted grid-compatible diagonal orientations");
    }
    for(int seed=0;seed<8;++seed){
        Generation rectangle;rectangle.seed=seed;rectangle.isSeedRandom=false;rectangle.generate(picker.mapTemplate,128,80);
        require(completeAxis(rectangle,128,80)<2,"Rectangle initializer chose a diagonal");
    }
}

static Result run(const Json& document,long long seed,int width=128,int height=128,bool asynchronous=false){
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(document),"Cannot load mode: "+templateValidationError(document));
    Generation g;g.seed=seed;g.isSeedRandom=false;
    Result out;
    auto reflection=picker.mapTemplate->embeddingReflection;
    std::map<int,int> partners{{-1,-1}};
    if(reflection){const auto& ids=picker.mapTemplate->embeddingTopology->ids;
        for(size_t i=0;i<ids.size();++i)partners[ids[i].getID()]=ids[reflection->partner[i]].getID();}
    DoubleVector2 normal{1,0};
    const auto mode=picker.mapTemplate->symmetryMode;
    auto check=[&]{
        if(!reflection)return 0.0;
        double error=0;
        for(auto [a,b]:partners)if(a>=0){
            const auto p=g.plane.getSpot(a).getCoords(),q=g.plane.getSpot(b).getCoords();
            const double dot=(p.x-width/2.0)*normal.x+(p.y-height/2.0)*normal.y;
            error=std::max(error,std::hypot(p.x-2*dot*normal.x-q.x,p.y-2*dot*normal.y-q.y));
        }
        if(mode!=SymmetryMode::Faces)require(error<1e-8,"Embedding constraints were lost");
        auto p=[&](int id){return g.plane.getSpot(id).getCoords();};
        require(inside(p(5),p(10),p(12),p(16))==inside(p(15),p(11),p(13),p(17)),"Corresponding face choices changed during relaxation");
        require(g.plane.findEdgeIntersections().empty(),"Embedding introduced a crossing");
        return error;
    };
    if(asynchronous){
        require(g.startAsync(picker.mapTemplate,width,height),"Cannot start complete async generation");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        while(g.isRunning()&&std::chrono::steady_clock::now()<deadline){g.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(!g.isRunning(),"Complete async generation timed out");
        require(g.generationError.empty(),g.generationError);
    }else{
        g.generate(picker.mapTemplate,width,height);
        if(reflection){
            for(auto [a,b]:partners)if(a>=0&&a!=b){
                const auto p=g.plane.getSpot(a).getCoords(),q=g.plane.getSpot(b).getCoords();
                const double d=std::hypot(p.x-q.x,p.y-q.y);normal={(p.x-q.x)/d,(p.y-q.y)/d};break;
            }
            require(check()<1e-8,"Face selection did not start with a mirrored drawing");
        }
        bool seedsChecked=false;
        g.runIteration([&](const Generation& state){
            if(state.generationStage==GenerationStage::EMBED)out.drift=std::max(out.drift,check());
            if(mode==SymmetryMode::Complete&&state.generationStage==GenerationStage::ZONE_BLOAT&&!seedsChecked){
                std::map<int,int> counts;
                for(auto id:*g.grid)++counts[id.getID()];
                for(auto [id,other]:partners)if(id>=0)require(counts[id]>0,"Mirroring deleted an axis or paired node seed");
                seedsChecked=true;
            }
            return true;
        });
    }
    require(g.is_done,"Generation did not finish");
    if(mode==SymmetryMode::Faces)require(out.drift>.01,"Face-only mode kept full coordinate symmetry enabled");
    if(mode==SymmetryMode::Complete)out.axis=completeAxis(g,width,height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const int id=g.grid->getTileID(x,y).getID();
        const int resource=int(g.spiceMap.get(x,y));
        out.tiles.insert(out.tiles.end(),{id,resource});
    }
    if(mode==SymmetryMode::Complete)checkComplete(g,partners,width,height);
    return out;
}

int main(int argc,char**argv){
    try{
        diagonalCases();
        require(argc>1,"Pass Spice Scout fixture");Json base;std::ifstream(argv[1])>>base;
        Json none=base,faces=base,embedding=base,complete=base;
        none["embedding"]={{"symmetry","none"}};faces["embedding"]={{"symmetry","faces"}};
        embedding["embedding"]={{"symmetry","embedding"}};complete["embedding"]={{"symmetry","complete"}};
        require(run(base,42).tiles==run(none,42).tiles,"No symmetry changed existing generation");
        auto legacy=base;legacy["embedding"]={{"symmetrical",true}};
        require(run(legacy,42).tiles==run(embedding,42).tiles,"Legacy checkbox no longer means Embedding symmetry");
        for(long long seed:{2604084809LL,12419LL}){
            run(faces,seed);
            const auto first=run(complete,seed);
            require(first.tiles==run(complete,seed).tiles,"Complete symmetry is not reproducible");
        }
        for(auto& node:complete["nodes"])if(node["id"]==5||node["id"]==15)
            node["data"]["magnet"]={{"force",5},{"position",{{"x",node["id"]==5?.1:.9},{"y",.102778}}}};
        complete["magnets"]={{"randomRotation",true}};
        run(complete,2604084809LL);
        run(complete,42,128,80);
        const auto synchronous=run(complete,12419);
        require(synchronous.tiles==run(complete,12419,128,128,true).tiles,"Async complete generation differs from synchronous output");
        auto asymmetricMagnets=complete;asymmetricMagnets["embedding"]["symmetry"]="faces";
        for(auto& node:asymmetricMagnets["nodes"])if(node["id"]==15)node["data"]["magnet"]["position"]["y"]=.4;
        run(asymmetricMagnets,42);
        auto mismatched=complete;mismatched["magnets"]["randomRotation"]=false;
        for(auto& node:mismatched["nodes"])if(node["id"]==5)node["data"]["radius"]=3;
        for(auto mode:{"faces","embedding","complete"}) {
            mismatched["embedding"]["symmetry"]=mode;
            require(templateValidationError(mismatched).find("matching")!=std::string::npos,
                "Symmetry selected a reflection with unmatched paired data");
            require(templateValidationError(mismatched,false).empty(),"Data mismatch prevents saving an editable draft");
            TemplateEditor warning;require(warning.replace(mismatched),warning.error());
            require(warning.symmetryWarning().find("matching")!=std::string::npos,"Editor omitted the symmetry data warning");
        }
        for(Json invalid:{Json(true),Json(nullptr),Json(1),Json("unknown"),Json::array()}){
            auto bad=base;bad["embedding"]={{"symmetry",invalid}};
            require(!templateValidationError(bad,false).empty(),"Invalid symmetry mode accepted");
        }
        TemplateEditor editor;require(editor.replace(base),editor.error());
        for(auto mode:{SymmetryMode::None,SymmetryMode::Faces,SymmetryMode::Embedding,SymmetryMode::Complete}){
            auto doc=editor.document();doc["embedding"]={{"symmetry",symmetryKey(mode)},{"customMetadata",17}};
            const auto before=editor.document();require(editor.replace(doc),editor.error());
            require(editor.symmetryWarning().empty(),editor.symmetryWarning());
            editor.undo();require(editor.document()==before,"Mode change cannot be undone");editor.redo();
            const auto path=std::filesystem::current_path()/"symmetry-modes-regression.json";
            require(editor.save(path),editor.error());TemplateEditor loaded;
            require(loaded.load(path)&&loaded.document()==editor.document(),"Mode or metadata lost on save/reload");
            std::filesystem::remove(path);
        }
        std::cout<<"All four modes, face freedom, tile-exact mirroring, walls, blending, rotation, async, compatibility and history passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
