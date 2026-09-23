#include "stdafx.h"
#include "App/Control/ControlCommands.h"

#ifdef _EDITOR

#include "App/Control/ControlMapArguments.h"
#include "Assets/EditorText.h"
#include "Core/EditorCamera.h"
#include "Core/ViewCapture.h"
#include "MapInspect/Image.h"
#include "MapInspect/PngFile.h"

#include "json.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace
{
using App::Control::Act;
using App::Control::EncodeError;
using App::Control::EncodeResult;
using App::Control::ErrorCode;
using App::Control::Request;
using nlohmann::json;
namespace Arguments = App::Control::MapArguments;
namespace Inspect = Editor::MapInspect;

constexpr const char* PNG_EXTENSION = ".png";
constexpr const char* CAPTURE_FOLDER = "map-captures";
constexpr int RGB_CHANNELS = 3;
// A few frames for the capture and its read-back; the allowance covers a slow GPU.
constexpr std::chrono::milliseconds CaptureDeadline{5000};

bool HasPngExtension(const std::string& path)
{
    const std::string extension = Editor::Text::PathToUtf8(Editor::Text::Utf8Path(path).extension());
    return Editor::Text::EqualIgnoringCase(extension, PNG_EXTENSION);
}

// What one editor capture asked for.
struct CaptureRequest
{
    bool clean = false;
    std::optional<Inspect::CellRect> region;
    std::filesystem::path file;
};

bool ReadCaptureRequest(const Request& request, CaptureRequest& capture, std::string& error)
{
    if (request.Has("clean") && !request.GetBool("clean", capture.clean))
    {
        error = "`clean` is true or false";
        return false;
    }
    if (request.Has("region"))
    {
        Inspect::CellRect area;
        if (!Arguments::ReadArea(request, "region", area, error))
            return false;
        capture.region = area;
    }
    std::string requested;
    if (request.Has("out") && (!request.GetString("out", requested) || !HasPngExtension(requested)))
    {
        error = "`out` is a .png path: clean and region captures are written as PNG";
        return false;
    }
    capture.file = requested.empty()
                       ? Arguments::DefaultOutput(CAPTURE_FOLDER, "capture-" + Arguments::UniqueStamp() + PNG_EXTENSION)
                       : Arguments::ResolvePath(requested);
    return true;
}

Inspect::Image ToImage(mu::FramePixels&& pixels)
{
    Inspect::Image image;
    image.width = static_cast<int>(pixels.width);
    image.height = static_cast<int>(pixels.height);
    image.channels = RGB_CHANNELS;
    image.pixels = std::move(pixels.rgb);
    return image;
}

// screenshot through Editor::ViewCapture: waits for the capture frame, crops it to
// the region, writes the PNG.
class EditorCaptureAct : public Act
{
public:
    explicit EditorCaptureAct(CaptureRequest capture) : m_capture(std::move(capture)) {}

    // Destroyed before its frame arrived (timeout, caller gone): the capture is
    // dropped, so the next one gets a frame of its own.
    ~EditorCaptureAct() override
    {
        if (!m_finished)
            Editor::ViewCapture::Cancel();
    }

    [[nodiscard]] std::string_view Name() const override
    {
        return "screenshot";
    }
    [[nodiscard]] bool IsAct() const override
    {
        return false;
    }
    [[nodiscard]] std::optional<std::chrono::milliseconds> Deadline() const override
    {
        return CaptureDeadline;
    }

    [[nodiscard]] Status Tick(std::string& response) override
    {
        mu::FramePixels pixels;
        const Editor::ViewCapture::Result result = Editor::ViewCapture::Collect(pixels);
        if (result == Editor::ViewCapture::Result::Waiting)
            return Status::Running;
        m_finished = true;
        response = result == Editor::ViewCapture::Result::Ready
                       ? Write(ToImage(std::move(pixels)))
                       : EncodeError(EncodedId(), ErrorCode::Failed, "the frame could not be read back");
        return Status::Finished;
    }

private:
    std::string Write(const Inspect::Image& frame) const
    {
        Inspect::Image image = frame;
        json result;
        if (m_capture.region)
        {
            const auto box = Editor::Camera::CaptureBoxOf(*m_capture.region, frame.width, frame.height);
            if (!box)
                return EncodeError(EncodedId(), ErrorCode::NotInView, "the region is not in view");
            image = Inspect::Crop(frame, box->x, box->y, box->width, box->height);
            result["region"] = Arguments::AreaJson(*m_capture.region);
            result["crop"] = json::array({box->x, box->y, box->width, box->height});
        }

        std::string error;
        if (!Inspect::WritePng(m_capture.file, image, error))
            return EncodeError(EncodedId(), ErrorCode::Failed, error);
        result["path"] = Arguments::PathJson(m_capture.file);
        result["width"] = image.width;
        result["height"] = image.height;
        result["frame"] = json::array({frame.width, frame.height});
        result["clean"] = m_capture.clean;
        return EncodeResult(EncodedId(), result.dump());
    }

    CaptureRequest m_capture;
    bool m_finished = false;
};
} // namespace

namespace App::Control::Commands
{
bool WantsEditorScreenshot(const Request& request)
{
    bool clean = false;
    std::string out;
    return (request.GetBool("clean", clean) && clean) || request.Has("region") ||
           (request.GetString("out", out) && HasPngExtension(out));
}

std::string EditorScreenshot(const Request& request, std::unique_ptr<Act>& act)
{
    CaptureRequest capture;
    std::string error;
    if (!ReadCaptureRequest(request, capture, error))
        return EncodeError(request.EncodedId(), ErrorCode::BadRequest, error);

    const auto overlay = capture.clean ? Editor::ViewCapture::Overlay::Hidden : Editor::ViewCapture::Overlay::Shown;
    if (!Editor::ViewCapture::Request(overlay))
        return EncodeError(request.EncodedId(), ErrorCode::Busy, "another capture is still being taken");

    act = std::make_unique<EditorCaptureAct>(std::move(capture));
    return {};
}
} // namespace App::Control::Commands

#endif // _EDITOR
