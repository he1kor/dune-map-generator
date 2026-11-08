#pragma once

#include "template_editor.h"
#include "imgui.h"
#include <functional>
#include <map>
#include <set>
#include <vector>

class TemplatePicker;
class Generation;
class QuickSettings;

class TemplateEditorUI {
public:
    TemplateEditorUI(TemplateEditor& editor, TemplatePicker& picker, Generation& generation, QuickSettings& settings);
    void build();
    void buildDialogs();
    bool requestClose();
    bool closeApproved() const { return closeApproved_; }

private:
    TemplateEditor& editor_;
    TemplatePicker& picker_;
    Generation& generation_;
    QuickSettings& settings_;
    ImVec2 camera_{0, 0};
    float zoom_ = 1;
    bool fit_ = true;
    enum class Mode { Selection, Edge, Magnet, Boundary };
    Mode mode_ = Mode::Selection;
    int connectionStart_ = -1;
    int selectedNode_ = -1;
    std::set<int> selectedNodes_;
    int selectedEdge_ = -1;
    int draggingNode_ = -1;
    TemplatePoint dragOffset_;
    TemplatePoint dragStart_;
    std::map<int, TemplatePoint> dragPositions_;
    bool moving_ = false;
    int magnetNode_ = -1;
    bool magnetDragging_ = false;
    double magnetStartForce_ = 1;
    float magnetStartDistance_ = 0;
    int toggleOnRelease_ = -1;
    bool selecting_ = false;
    TemplatePoint selectionStart_;
    std::set<int> selectionBase_;
    ImVec2 canvasOrigin_, canvasSize_;
    TemplatePoint contextPoint_;
    int contextNode_ = -1;
    int contextEdge_ = -1;
    std::string message_;
    std::string dialogError_;
    enum class Dialog { None, Globals, Node, Edge };
    Dialog dialog_ = Dialog::None;
    bool openDialog_ = false;
    TemplateEditor::Json draft_;
    TemplateEditor::Json dialogInitial_;
    bool dismissDialog_ = false;
    struct DraftState { TemplateEditor::Json document; int id; };
    struct DraftChange { DraftState before, after; ImGuiID widget; };
    std::vector<DraftChange> draftUndo_, draftRedo_;
    ImGuiID draftGroup_ = 0;
    int editingNode_ = -1;
    int editingEdge_ = -1;
    int editedId_ = -1;
    std::function<void()> pendingAction_;
    bool openDiscard_ = false;
    bool closeApproved_ = false;
    bool openHelp_ = false;
    struct Toast { size_t id; std::string text; double expires; };
    std::vector<Toast> toasts_;
    size_t nextToastId_ = 0;

    void toolbar();
    void canvas();
    void globalFields();
    void nodeFields();
    void edgeFields();
    void edit(Dialog dialog, int index = -1);
    bool save(bool saveAs);
    void guardChanges(std::function<void()> action);
    void deleteSelection();
    void fitView();
    void autoPlace();
    void resetInteraction();
    void finishMove();
    void finishMagnet();
    Mode activeMode() const;
    void load();
    void createNew();
    void useForGeneration();
    void history(bool redo);
    void shortcuts();
    void copyData();
    void pasteData();
    bool canPasteData() const;
    void notify(std::string text);
    void notifications();
    void help();
    bool dialogChanged() const;
    bool applyDialog();
    void draftHistory(bool redo);
    ImVec2 screen(TemplatePoint point) const;
    TemplatePoint world(ImVec2 point) const;
};
