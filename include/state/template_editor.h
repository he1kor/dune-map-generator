#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace planar { struct Topology; }

struct TemplatePoint {
    float x = 0;
    float y = 0;
    bool operator==(const TemplatePoint&) const = default;
};

struct TemplateMagnetFrame {
    TemplatePoint center;
    double halfExtent = 128;
    double scale = 1;
};

// The editable JSON is kept separate from MapTemplate. Visualizer metadata and
// optional/unknown fields survive edits without entering generation data.
class TemplateEditor {
public:
    using Json = nlohmann::json;
    TemplateEditor();
    void createNew();
    bool replace(Json candidate, const std::string& label = "Edit settings");
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path);
    const Json& document() const { return document_; }
    const std::filesystem::path& path() const { return path_; }
    bool dirty() const;
    const std::string& error() const { return error_; }
    int addNode(TemplatePoint position);
    bool connect(int first, int second);
    void removeNode(int id);
    void removeConnection(size_t index);
    bool renameNode(int oldId, int newId, Json& candidate) const;
    void setPosition(int id, TemplatePoint position);
    TemplatePoint position(int id) const;
    static constexpr double minMagnetForce = 0.1;
    static constexpr double defaultMagnetForce = 1.0;
    static constexpr double maxMagnetForce = 100.0;
    static TemplateMagnetFrame magnetFrame(const Json& document);
    TemplateMagnetFrame magnetFrame() const { return magnetFrame(document_); }
    bool isMagnet(int id) const;
    double magnetForce(int id) const;
    TemplatePoint magnetTarget(int id) const;
    bool magnetInsideFrame(int id) const;
    void setMagnet(int id, bool enabled);
    void setMagnetForce(int id, double force);
    void setMagnetFrameScale(double scale);
    const Json* node(int id) const;
    Json copyNodeData(int id) const;
    Json copyConnectionData(size_t index) const;
    bool pasteNodeData(const Json& payload, const std::vector<int>& ids);
    bool pasteConnectionData(const Json& payload, size_t index);
    static bool isDataClipboard(const Json& payload, const char* kind);
    static float halfSize(const Json& node);
    const std::string& topologyWarning() const;
    bool autoPlace();
    static double patternLength(const Json& edge);
    void beginAction(const std::string& label);
    void endAction();
    bool canUndo() const { return !undo_.empty() && actionDepth_ == 0; }
    bool canRedo() const { return !redo_.empty() && actionDepth_ == 0; }
    std::string undo();
    std::string redo();
    const std::string& undoLabel() const;
    const std::string& redoLabel() const;
    static std::string describeChanges(const Json& from, const Json& to);

private:
    Json document_;
    std::filesystem::path path_;
    std::string error_;
    struct Snapshot { Json document; std::filesystem::path path; size_t session = 0; };
    struct Change { Snapshot before, after; std::string label; };
    struct Action {
        TemplateEditor& editor;
        Action(TemplateEditor& editor, const std::string& label) : editor(editor) { editor.beginAction(label); }
        ~Action() { editor.endAction(); }
    };
    std::map<std::filesystem::path, Json> saved_;
    std::vector<Change> undo_, redo_;
    int actionDepth_ = 0;
    Snapshot actionBefore_;
    std::string actionLabel_;
    size_t session_ = 0;
    size_t nextSession_ = 0;
    mutable Json topologySignature_;
    mutable std::shared_ptr<const planar::Topology> topology_;
    mutable std::string topologyWarning_;
    void updateTopology() const;
    static std::string checkPositions(const Json& candidate);
    static void addMissingPositions(Json& candidate);
    static void updateMagnets(Json& candidate);
};
