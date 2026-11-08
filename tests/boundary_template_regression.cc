#include "template_editor.h"
#include "template_picker.h"
#include "template_validation.h"
#include "generation.h"
#include "planar_symmetry.h"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using Json=nlohmann::json;
static void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}

static void checkScoutLatency(const std::filesystem::path& fixture) {
    Json document;std::ifstream(fixture)>>document;
    // Reproduce the authored boundary contour without reading or changing the
    // user's runtime template: move two perimeter subdivisions through 5/15.
    auto& edges=document["symmetrical_edges"].get_ref<Json::array_t&>();
    std::erase_if(edges,[](const Json& edge){
        const int a=edge["ID_first"],b=edge["ID_second"];
        return std::pair{std::min(a,b),std::max(a,b)}==std::pair{10,12}
            ||std::pair{std::min(a,b),std::max(a,b)}==std::pair{11,13};
    });
    for(auto& edge:edges) {
        const int a=edge["ID_first"],b=edge["ID_second"];
        const auto pair=std::pair{std::min(a,b),std::max(a,b)};
        if(pair==std::pair{12,16}){edge["ID_first"]=5;edge["ID_second"]=12;}
        if(pair==std::pair{13,17}){edge["ID_first"]=13;edge["ID_second"]=15;}
    }
    document["embedding"]={{"symmetry","complete"}};
    TemplateEditor editor;require(editor.replace(document),editor.error());
    editor.setBoundaryPlacement({3,4,5,6,7,8,12,13,14,15,16,17},BoundaryPlacement::Outer);
    require(editor.topologyWarning().empty(),editor.topologyWarning());
    editor.setBoundaryPlacement({5},BoundaryPlacement::Inner);
    const auto start=std::chrono::steady_clock::now();
    const auto warning=editor.topologyWarning();
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    require(!warning.empty()&&warning.find("limit")==std::string::npos,
        "Spice Scout contradiction was not certified without exhaustive search");
    require(elapsed<250,"Boundary misclick blocked the editor");
    for(int frame=0;frame<20;++frame)require(editor.topologyWarning()==warning,"Cached warning changed");
    require(editor.symmetryWarning().empty(),"Invalid boundary rules entered symmetry search");
    const auto validation=templateValidationError(editor.document());
    require(!validation.empty()&&validation.find("limit")==std::string::npos,"Full validation did not certify the contour conflict");
    editor.undo();require(editor.topologyWarning().empty(),"Undo left stale boundary conflict");
    std::cout<<"Spice Scout zone 5 Inner check: "<<elapsed<<" ms\n";
}

static void checkGeneration(Json document,const char* mode) {
    document["embedding"]={{"symmetry",mode}};
    TemplatePicker picker;
    const auto validation=templateValidationError(document);
    require(validation.empty(),std::string(mode)+": "+validation);
    require(picker.tryReadEdgeGraph(document),"Could not load valid boundary template");
    require(!picker.mapTemplate->nodeBoundary.empty(),"Boundary rules were lost during loading");
    auto graph=picker.mapTemplate->embeddingTopology;
    for(int seed:{4,12419})for(auto size:{std::pair{32,32},std::pair{64,64},std::pair{128,64}}) {
        Generation generation;
        generation.isSeedRandom=false;generation.seed=seed;
        generation.generate(picker.mapTemplate,size.first,size.second);
        int checks=0;
        generation.runIteration([&](const Generation& state){
            if(state.generationStage==GenerationStage::EMBED) {
                std::vector<DoubleVector2> points;
                for(auto id:graph->ids)points.push_back(state.plane.getSpot(id).getCoords());
                require(planar::validBoundaryDrawing(points,*graph),"Embedding broke an authored boundary rule");
                ++checks;
            }
            return true;
        });
        require(generation.is_done&&checks>0,"Boundary-constrained generation did not finish");
        std::vector<DoubleVector2> raster;
        for(auto id:graph->ids) {
            const auto p=generation.plane.getSpot(id).getCoords();
            raster.emplace_back(std::floor(p.x),std::floor(p.y));
        }
        require(planar::validBoundaryDrawing(raster,*graph),"Final seeds violated boundary rules");
    }
}

int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected scratch template and Spice Scout fixture paths");
        checkScoutLatency(argv[2]);
        TemplateEditor editor;
        const int a=editor.addNode({-128,-128}),b=editor.addNode({128,-128});
        const int c=editor.addNode({0,128}),leaf=editor.addNode({0,0});
        editor.connect(a,b);editor.connect(b,c);editor.connect(c,a);editor.connect(a,leaf);
        const auto original=editor.document();
        editor.setBoundaryPlacement({a,b,c},BoundaryPlacement::Outer);
        editor.setBoundaryPlacement({leaf},BoundaryPlacement::Inner);
        require(editor.topologyWarning().empty(),editor.topologyWarning());
        const auto constrained=editor.document();
        require(editor.undo().find("boundary")!=std::string::npos&&editor.boundaryPlacement(leaf)==BoundaryPlacement::Any,"Boundary undo failed");
        editor.redo();require(editor.document()==constrained,"Boundary redo failed");
        editor.cycleBoundaryPlacement({a,leaf});
        require(editor.boundaryPlacement(a)==BoundaryPlacement::Outer&&editor.boundaryPlacement(leaf)==BoundaryPlacement::Outer,"Mixed selection did not become Outer");
        editor.undo();
        editor.cycleBoundaryPlacement({a,b,c});
        require(!editor.topologyWarning().empty(),"Contradictory rules did not invalidate cached topology");
        require(templateValidationError(editor.document(),false).empty(),"Incompatible draft became uneditable");
        require(!templateValidationError(editor.document()).empty(),"Incompatible draft reached generation");
        editor.undo();require(editor.topologyWarning().empty(),"Undo left a stale boundary warning");

        const auto frame=editor.magnetFrame();
        editor.setMagnet(leaf,true);
        const auto magnets=editor.document()["nodes"][3]["data"]["magnet"];
        editor.cycleBoundaryPlacement({leaf});editor.undo();
        require(editor.document()["nodes"][3]["data"]["magnet"]==magnets,"Boundary editing moved a magnet target");
        require(editor.magnetFrame().halfExtent==frame.halfExtent,"Boundary editing resized map frame");
        editor.setMagnet(leaf,false);
        require(editor.autoPlace(),editor.error());
        TemplatePicker picker;
        require(picker.tryReadEdgeGraph(editor.document()),"Auto-placed constrained template is invalid");
        auto graph=picker.mapTemplate->embeddingTopology;
        std::vector<DoubleVector2> diagram;
        for(auto id:graph->ids){const auto p=editor.position(id.getID());diagram.emplace_back(p.x,p.y);}
        require(planar::validBoundaryDrawing(diagram,*graph),"Auto place ignored boundary rules");
        const auto snapshotRules=picker.mapTemplate->nodeBoundary;
        editor.setBoundaryPlacement({leaf},BoundaryPlacement::Outer);
        require(picker.mapTemplate->nodeBoundary==snapshotRules,"Editing changed the selected generation snapshot");
        editor.undo();

        const auto payload=editor.copyNodeData(leaf);
        require(editor.pasteNodeData(payload,{b}),editor.error());
        require(editor.boundaryPlacement(b)==BoundaryPlacement::Inner,"Copy/paste lost boundary data");
        editor.undo();
        auto renamed=editor.document();
        require(editor.renameNode(leaf,100,renamed)&&editor.replace(renamed),"Cannot rename constrained node");
        require(editor.boundaryPlacement(100)==BoundaryPlacement::Inner,"Rename lost boundary rule");
        editor.undo();
        const std::filesystem::path scratch=argv[1];
        require(editor.save(scratch),editor.error());
        TemplateEditor reopened;
        require(reopened.load(scratch)&&reopened.document()==editor.document(),"Boundary save/load did not round-trip");
        std::filesystem::remove(scratch);

        for(const Json value:{Json(true),Json(2),Json(nullptr),Json("middle"),Json::object()}) {
            auto malformed=constrained;
            malformed["nodes"][0]["data"]["boundary"]=value;
            require(!templateValidationError(malformed,false).empty(),"Invalid boundary value accepted");
            require(!picker.tryReadEdgeGraph(malformed),"Invalid boundary value loaded");
        }
        auto plain=original;
        require(picker.tryReadEdgeGraph(plain),"Legacy template rejected");
        require(picker.mapTemplate->nodeBoundary.empty()&&picker.mapTemplate->embeddingTopology->nodeBoundary.empty(),"Legacy template acquired constraints");
        plain["nodes"][0]["data"]["boundary"]="any";
        require(picker.tryReadEdgeGraph(plain)&&picker.mapTemplate->nodeBoundary.empty(),"Explicit Any is not unrestricted");

        for(const char* mode:{"none","faces","embedding","complete"})checkGeneration(constrained,mode);
        auto outside=constrained;outside["nodes"][3]["data"]["boundary"]="outer";
        for(const char* mode:{"none","faces","embedding","complete"})checkGeneration(outside,mode);
        // Any is compatible with a reflected partner's explicit requirement.
        auto partial=constrained;partial["nodes"][1]["data"].erase("boundary");
        checkGeneration(partial,"embedding");
        std::cout<<"Boundary validation, cache, undo, clipboard, auto place, snapshots and every symmetry mode passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
