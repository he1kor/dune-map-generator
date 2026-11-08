#include "generation.h"
#include "template_picker.h"
#include "template_editor.h"
#include "template_validation.h"
#include "planar_symmetry.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

using Json = nlohmann::json;
static void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
static double distance(DoubleVector2 a, DoubleVector2 b) { return std::hypot(a.x-b.x,a.y-b.y); }
static double pointEdgeDistance(DoubleVector2 p, DoubleVector2 a, DoubleVector2 b) {
    const DoubleVector2 d{b.x-a.x,b.y-a.y};
    const double squared=d.x*d.x+d.y*d.y;
    const double t=squared>0?std::clamp(((p.x-a.x)*d.x+(p.y-a.y)*d.y)/squared,0.0,1.0):0;
    return distance(p,{a.x+t*d.x,a.y+t*d.y});
}
static std::set<std::pair<int,int>> pixels(DoubleVector2 a, DoubleVector2 b) {
    int x=int(a.x),y=int(a.y),endX=int(b.x),endY=int(b.y);
    const bool steep=std::abs(endY-y)>std::abs(endX-x);
    if(steep){std::swap(x,y);std::swap(endX,endY);}
    if(x>endX){std::swap(x,endX);std::swap(y,endY);}
    const int dx=endX-x,dy=std::abs(endY-y),step=y<endY?1:-1;
    int error=dx/2;std::set<std::pair<int,int>> result;
    for(;x<=endX;++x){result.emplace(steep?y:x,steep?x:y);error-=dy;if(error<0){y+=step;error+=dx;}}
    return result;
}
static bool sameCycle(const std::vector<size_t>& a,const std::vector<size_t>& b) {
    if(a.size()!=b.size())return false;
    if(a.empty())return true;
    for(size_t offset=0;offset<a.size();++offset){
        bool same=true;
        for(size_t i=0;i<a.size();++i)if(a[i]!=b[(i+offset)%b.size()]){same=false;break;}
        if(same)return true;
    }
    return false;
}
static bool inside(DoubleVector2 p, DoubleVector2 a, DoubleVector2 b, DoubleVector2 c) {
    const int x=planar::orientation(a,b,p), y=planar::orientation(b,c,p), z=planar::orientation(c,a,p);
    return (x>0&&y>0&&z>0)||(x<0&&y<0&&z<0);
}
static void duelCases() {
    Json document;std::ifstream(std::filesystem::path(__FILE__).parent_path()/"fixtures/duel.json")>>document;
    const auto originalEdges=document["symmetrical_edges"];
    std::map<std::string,std::set<int>> orientations;
    std::map<std::string,std::set<int>> finalOrientations;
    for(auto mode:{"faces","embedding","complete"}) for(int size:{64,128})
        for(long long seed:{3604091217LL,3693219765LL,42LL}) {
        document["embedding"]={{"symmetry",mode}};
        TemplatePicker picker;require(picker.tryReadEdgeGraph(document),templateValidationError(document));
        const auto t=picker.mapTemplate->embeddingTopology;
        require(t->edges.size()==originalEdges.size(),"Duel symmetry changed the edge set");
        const auto reflection=picker.mapTemplate->embeddingReflection;
        auto partner=[&](int id){return t->ids[reflection->partner[std::find(t->ids.begin(),t->ids.end(),Identifiable(id))-t->ids.begin()]].getID();};
        require(partner(4)==4&&partner(6)==6&&partner(1)==2&&partner(3)==5&&partner(7)==8,
            "Duel selected a topology-only axis exchanging different zone types");
        Generation g;g.seed=seed;g.isSeedRandom=false;g.generate(picker.mapTemplate,size,size);
        auto point=[&](int id){return g.plane.getSpot(id).getCoords();};
        const auto a=point(4),b=point(6);
        orientations[mode].insert(int(std::atan2(a.y-b.y,a.x-b.x)*1000));
        Generation repeated;repeated.seed=seed;repeated.isSeedRandom=false;repeated.generate(picker.mapTemplate,size,size);
        for(auto id:t->ids)require(distance(point(id.getID()),repeated.plane.getSpot(id).getCoords())<1e-12,
            "Same seed does not reproduce the initial orientation");
        auto check=[&] {
            bool same=true,reversed=true;
            for(size_t v=0;v<t->ids.size();++v){
                auto order=t->rotation[v];const auto p=point(t->ids[v].getID());
                std::sort(order.begin(),order.end(),[&](size_t a,size_t b){
                    const auto q=point(t->ids[a].getID()),r=point(t->ids[b].getID());
                    return std::atan2(q.y-p.y,q.x-p.x)<std::atan2(r.y-p.y,r.x-p.x);
                });
                same &= sameCycle(order,t->rotation[v]);std::reverse(order.begin(),order.end());
                reversed &= sameCycle(order,t->rotation[v]);
            }
            require(same||reversed,"Duel symmetry changed the existing cyclic edge order");
            for(int id:{1,2,3,5,6})require(!inside(point(id),point(4),point(7),point(8)),
                "Duel's already symmetric faces were replaced by the containing triangle 4-7-8");
            require(g.plane.findEdgeIntersections().empty(),"Duel has a continuous edge crossing");
            for(auto id:t->ids)for(auto [a,b]:t->edges)if(id!=t->ids[a]&&id!=t->ids[b])
                require(pointEdgeDistance(point(id.getID()),point(t->ids[a].getID()),point(t->ids[b].getID()))>1.4,
                    "Duel symmetry collapsed a node onto another edge");
            for(auto [a,b]:{std::pair{7,1},std::pair{8,2}}){
                const auto first=pixels(point(a),point(4)),second=pixels(point(a),point(b));
                for(auto pixel:first)if(second.contains(pixel))
                    require(std::max(std::abs(pixel.first-int(point(a).x)),std::abs(pixel.second-int(point(a).y)))<=2,
                        "Duel raster edges overlap beyond their shared endpoint");
            }
        };
        check();g.runIteration([&](const Generation& state){if(state.generationStage==GenerationStage::EMBED)check();return true;});
        require(g.is_done,"Duel symmetry did not finish generation");check();
        const auto finalA=point(4),finalB=point(6);
        finalOrientations[mode].insert(int(std::atan2(finalA.y-finalB.y,finalA.x-finalB.x)*1000));
    }
    require(orientations["faces"].size()>=3&&orientations["embedding"].size()>=3,
        "Symmetry optimizes away seeded orientation");
    require(finalOrientations["faces"].size()>=3&&finalOrientations["embedding"].size()>=3,
        "Symmetric refinement erased orientation variety between seeds");
    TemplatePicker picker;require(picker.tryReadEdgeGraph(document),templateValidationError(document));
    const auto t=picker.mapTemplate->embeddingTopology;
    planar::Layout squeezed(t,std::vector<double>(8,1),{},0,0,128,128,42,{},picker.mapTemplate->embeddingReflection);
    while(squeezed.isRunning()){
        std::vector<DoubleVector2> forces;
        for(auto p:squeezed.positions())forces.push_back({100*(64-p.x),100*(64-p.y)});
        squeezed.step(forces);
        const auto& points=squeezed.positions();
        for(size_t v=0;v<points.size();++v)for(auto [a,b]:t->edges)if(v!=a&&v!=b)
            require(pointEdgeDistance(points[v],points[a],points[b])>1.4,
                "Hostile symmetric forces bypassed the minimum clearance guard");
    }
    // This graph has the same topology but labels require the other axis.
    // Searching must continue after rejecting the first topological pairing.
    auto otherAxis=document;otherAxis["asymmetrical_edges"]=Json::array();
    for(auto& node:otherAxis["nodes"]){
        const int id=node["id"];
        node["data"]={{"radius",id==1?2.0:id==2?3.0:1.0},{"susceptibility",0.0},
            {"resources",{{"content",Json::array({{{"name",(id==3||id==7)?"rock":(id==5||id==8)?"sand":"spice"},
                {"startThreshold",0.0},{"endThreshold",1.0}}})}}}};
    }
    for(auto& edge:otherAxis["symmetrical_edges"])edge["data"]={{"passes",{{"minPassWidth",2},{"maxPassWidth",5},{"minWallLength",2},{"maxWallLength",5}}}};
    for(auto mode:{"faces","embedding","complete"}){
        otherAxis["embedding"]={{"symmetry",mode}};
        TemplatePicker alternative;require(alternative.tryReadEdgeGraph(otherAxis),templateValidationError(otherAxis));
        const auto graph=alternative.mapTemplate->embeddingTopology;
        const auto r=alternative.mapTemplate->embeddingReflection;
        auto partner=[&](int id){return graph->ids[r->partner[std::find(graph->ids.begin(),graph->ids.end(),Identifiable(id))-graph->ids.begin()]].getID();};
        require(partner(1)==1&&partner(2)==2&&partner(4)==6&&partner(3)==7&&partner(5)==8,
            "Symmetry ignores labels when choosing between valid topological axes");
    }
    std::cout<<"Duel face preservation, numerical clearance and raster edges: 18 mode/size/seed cases passed\n";
}
static std::shared_ptr<const planar::Topology> topology(size_t n, const std::vector<planar::Edge>& edges) {
    std::vector<Identifiable> ids;
    std::vector<std::pair<Identifiable,Identifiable>> links;
    for (size_t i=0;i<n;++i) ids.emplace_back(static_cast<int>(i));
    for (auto [a,b]:edges) links.emplace_back(static_cast<int>(a),static_cast<int>(b));
    return planar::makeTopology(ids,links);
}
static void geometryCases() {
    std::vector<std::pair<size_t,std::vector<planar::Edge>>> graphs{
        {1,{}},{2,{{0,1}}},{5,{{0,1},{1,2},{2,3},{3,4}}},
        {6,{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}}},
        {4,{{0,1},{0,2},{0,3},{1,2},{1,3},{2,3}}},
        {7,{{0,1},{0,2},{0,3},{0,4},{0,5},{0,6}}},
        {6,{{0,1},{1,2},{2,0},{3,4},{4,5},{5,3}}}};
    std::vector<planar::Edge> grid;
    for (size_t y=0;y<3;++y) for (size_t x=0;x<3;++x) {
        if(x<2)grid.emplace_back(3*y+x,3*y+x+1);
        if(y<2)grid.emplace_back(3*y+x,3*(y+1)+x);
    }
    graphs.push_back({9,grid});
    for (const auto& [n,edges]:graphs) {
        const auto t=topology(n,edges);
        const auto reflection=planar::findReflection(*t);
        require(reflection->partner.size()==n && planar::validDrawing(reflection->drawing,t->edges),"Invalid reflection certificate");
        std::cout<<"Geometry "<<n<<" nodes, "<<edges.size()<<" edges\n";
        for (size_t i=0;i<n;++i) {
            const auto p=reflection->drawing[i], q=reflection->drawing[reflection->partner[i]];
            require(distance({-p.x,p.y},q)<1e-9,"Certificate is not reflective");
        }
        for (auto [width,height]:{std::pair{128,128},std::pair{128,80}}) {
            std::vector<double> radii;
            for (size_t i=0;i<n;++i) radii.push_back(1+.1*i);
            planar::Layout layout(t,radii,{},0,0,width,height,42,{},reflection);
            const auto normal=layout.symmetryAxisNormal().value();
            const auto check=[&] {
                const auto& points=layout.positions();
                for (size_t i=0;i<n;++i) {
                    const auto p=points[i],q=points[reflection->partner[i]];
                    const double dot=(p.x-width/2.0)*normal.x+(p.y-height/2.0)*normal.y;
                    require(distance({p.x-2*dot*normal.x,p.y-2*dot*normal.y},q)<1e-8,"Relaxation broke reflection symmetry");
                }
                require(planar::validDrawing(points,t->edges),"Symmetric relaxation introduced a crossing");
            };
            check();
            while(layout.isRunning()) {
                std::vector<DoubleVector2> force(n);
                for(size_t i=0;i<n;++i)force[i]={.23*double(i+1),-.17*double(i)};
                layout.step(force);check();
            }
            require(planar::validRasterDrawing(layout.positions(),t->edges,0,0,width,height),"Symmetric drawing did not survive tile rounding");
        }
    }
    const auto asymmetric=topology(7,{{0,1},{0,2},{2,3},{0,4},{4,5},{5,6}});
    bool rejected=false;
    try{planar::findReflection(*asymmetric);}catch(const planar::NoReflection&){rejected=true;}
    require(rejected,"An asymmetric branching graph was reported symmetric");
    const auto localSwap=topology(9,{{0,1},{0,2},{2,3},{0,4},{4,5},{5,6},{4,7},{4,8}});
    rejected=false;
    try{planar::findReflection(*localSwap);}catch(const planar::NoReflection&){rejected=true;}
    require(rejected,"A local leaf swap was mistaken for a global reflection axis");
}

static std::vector<int> generate(const Json& document, long long seed, bool checkSymmetry, int width=128,int height=128) {
    TemplatePicker picker;require(picker.tryReadEdgeGraph(document),"Cannot load symmetry template: "+templateValidationError(document));
    const auto size=picker.mapTemplate->dimensionLimits.closest({width,height});
    width=size.width; height=size.height;
    Generation g;g.seed=seed;g.isSeedRandom=false;g.generate(picker.mapTemplate,width,height);
    std::shared_ptr<const planar::Reflection> reflection;
    DoubleVector2 normal{1,0};
    if(checkSymmetry) {
        const auto targets=picker.mapTemplate->magnetsForGeneration(seed,width,height);
        reflection=planar::findReflection(*picker.mapTemplate->embeddingTopology,targets,width,height);
        const auto& ids=picker.mapTemplate->embeddingTopology->ids;
        for(size_t i=0;i<ids.size();++i)if(i!=reflection->partner[i]) {
            const auto a=g.plane.getSpot(ids[i]).getCoords(),b=g.plane.getSpot(ids[reflection->partner[i]]).getCoords();
            const double d=distance(a,b);normal={(a.x-b.x)/d,(a.y-b.y)/d};break;
        }
    }
    auto check=[&] {
        if(!reflection)return;
        const auto& ids=picker.mapTemplate->embeddingTopology->ids;
        for(size_t i=0;i<ids.size();++i) {
            const auto a=g.plane.getSpot(ids[i]).getCoords(),b=g.plane.getSpot(ids[reflection->partner[i]]).getCoords();
            const double dot=(a.x-width/2.0)*normal.x+(a.y-height/2.0)*normal.y;
            require(distance({a.x-2*dot*normal.x,a.y-2*dot*normal.y},b)<1e-8,"Generator lost paired positions");
        }
        if(document.value("name",std::string{})=="Spice scout") {
            auto point=[&](int id){return g.plane.getSpot(id).getCoords();};
            require(inside(point(5),point(10),point(12),point(16))==inside(point(15),point(11),point(13),point(17)),
                "Zones 5 and 15 occupy non-corresponding triangle faces");
        }
        require(g.plane.findEdgeIntersections().empty(),"Generator accepted a symmetric crossing");
    };
    check();
    g.runIteration([&](const Generation& state){if(state.generationStage==GenerationStage::EMBED)check();return true;});
    require(g.is_done,"Symmetric generation did not finish");check();
    if(reflection)for(const auto& [id,magnet]:picker.mapTemplate->magnetsForGeneration(seed,width,height))
        if(magnet.force>=5)require(distance(g.plane.getSpot(id).getCoords(),{magnet.position.x*width,magnet.position.y*height})<8,
            "Symmetry prevented a strong compatible magnet from holding its target");
    std::vector<int> tiles;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){tiles.push_back(g.grid->getTileID(x,y).getID());tiles.push_back(int(g.spiceMap.get(x,y)));}
    return tiles;
}

int main(int argc,char** argv) {
    try {
        geometryCases();
        duelCases();
        require(argc>=2,"Pass Spice Scout fixture");Json base;std::ifstream(argv[1])>>base;
        auto symmetric=base;symmetric["embedding"]={{"symmetrical",true}};
        TemplatePicker picker;require(picker.tryReadEdgeGraph(symmetric),"Cannot read symmetric Spice Scout: "+templateValidationError(symmetric));
        const auto t=picker.mapTemplate->embeddingTopology;
        const auto reflection=picker.mapTemplate->embeddingReflection;
        for(auto [a,b]:{std::pair{5,15},std::pair{10,11},std::pair{12,13},std::pair{3,4}}) {
            auto index=std::find(t->ids.begin(),t->ids.end(),Identifiable(a))-t->ids.begin();
            require(t->ids[reflection->partner[index]].getID()==b,"Spice Scout's intended reflection was not detected");
        }
        for(long long seed:{2604084809LL,12419LL,42LL}) {
            auto tiles=generate(symmetric,seed,true);
            require(tiles==generate(symmetric,seed,true),"Symmetric generation is not reproducible");
            std::cout<<"Spice Scout symmetric seed "<<seed<<" passed\n";
        }
        auto magnets=symmetric;
        for(auto& node:magnets["nodes"])if(node["id"]==5||node["id"]==15)
            node["data"]["magnet"]={{"force",5},{"position",{{"x",node["id"]==5?.1:.9},{"y",.102778}}}};
        generate(magnets,2604084809LL,true);
        magnets["magnets"]={{"randomRotation",true}};
        generate(magnets,2604084809LL,true);
        generate(magnets,42,true,128,80);
        const auto source=std::filesystem::path(__FILE__).parent_path().parent_path();
        for(const auto* name:{"habbanya erg true 2.json","grid3x3 magnetic.json"}) {
            Json document;std::ifstream(source/"assets/templates"/name)>>document;
            document["embedding"]={{"symmetrical",true}};
            generate(document,12419,true);
            std::cout<<"Symmetric template "<<name<<" passed\n";
        }
        Json unequal;std::ifstream(source/"assets/templates/edge geometry example.json")>>unequal;
        unequal["embedding"]={{"symmetry","embedding"}};
        require(templateValidationError(unequal).find("matching")!=std::string::npos,
            "A topologically symmetric star with all different zone types was accepted");
        unequal["embedding"]["symmetry"]="none";generate(unequal,12419,false);
        auto conflict=magnets;
        for(auto& node:conflict["nodes"])if(node["id"]==15)node["data"]["magnet"]["position"]["y"]=.4;
        require(templateValidationError(conflict).find("magnet targets")!=std::string::npos,"Conflicting magnet targets did not produce a separate explanation");
        require(templateValidationError(conflict,false).empty(),"Conflicting symmetry cannot be kept as an editable draft");
        auto off=base;off["embedding"]={{"symmetrical",false}};
        require(generate(base,42,false)==generate(off,42,false),"Disabled symmetry changed generation");
        for(Json value:{Json(1),Json("true"),Json(nullptr),Json::array(),Json::object()}) {
            auto bad=base;bad["embedding"]={{"symmetrical",value}};
            require(!templateValidationError(bad,false).empty(),"Invalid symmetry flag accepted");
        }
        TemplateEditor editor;require(editor.replace(symmetric),editor.error());
        require(editor.symmetryWarning().empty(),"Symmetric graph shows a warning");
        require(editor.replace(conflict),editor.error());
        require(editor.symmetryWarning().find("magnet targets")!=std::string::npos,"Editor did not show the magnet compatibility warning");
        editor.undo();require(editor.document()==symmetric&&editor.symmetryWarning().empty(),"Undo did not clear symmetry warning");
        const auto saved=std::filesystem::current_path()/"symmetric-template-regression.json";
        require(editor.save(saved),editor.error());TemplateEditor reopened;
        require(reopened.load(saved)&&reopened.document()==editor.document(),"Save/reload lost symmetry settings");
        std::filesystem::remove(saved);
        TemplateEditor asymmetric;
        for(int i=0;i<7;++i)asymmetric.addNode({float(i*128),0});
        for(auto [a,b]:std::vector<std::pair<int,int>>{{1,2},{1,3},{3,4},{1,5},{5,6},{6,7}})asymmetric.connect(a,b);
        auto draft=asymmetric.document();draft["embedding"]={{"symmetrical",true}};
        require(asymmetric.replace(draft),asymmetric.error());
        require(asymmetric.topologyWarning().empty()&&!asymmetric.symmetryWarning().empty(),"Missing symmetry warning was confused with non-planarity");
        require(!templateValidationError(draft).empty(),"Generation accepted unsupported symmetry");
        asymmetric.undo();require(asymmetric.symmetryWarning().empty(),"Disabling symmetry leaves a stale warning");
        std::cout<<"Symmetry certificates, coupled relaxation, triangles, magnets, rotation, validation and history passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
