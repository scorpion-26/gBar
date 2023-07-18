#pragma once
#include <gtk/gtk.h>
#include <vector>
#include <memory>
#include <string>
#include <functional>

enum class Alignment
{
    Fill,
    Center,
    Left,
    Right,
};

struct Transform
{
    int size = -1;
    bool expand = true;
    Alignment alignment = Alignment::Fill;
    // Left/Top
    int marginBefore = 0;
    // Right/Bottom
    int marginAfter = 0;
};

enum class Orientation
{
    Vertical,
    Horizontal
};

struct Spacing
{
    uint32_t free = 0;
    bool evenly = false;
};

enum class TransitionType
{
    Fade,
    SlideLeft,
    SlideRight,
    SlideUp,
    SlideDown
};

struct Transition
{
    TransitionType type;
    uint32_t durationMS;
};

struct Quad
{
    double x, y, size;
};

struct SensorStyle
{
    double start = -90; // 0 = leftmost; -90 = topmost
    double strokeWidth = 4;
};

struct Range
{
    double min, max;
};

struct SliderRange : Range
{
    double step;
};

enum class TimerDispatchBehaviour
{
    ImmediateDispatch, // Call immediately after adding the timeout, then every x ms
    LateDispatch       // Call for the first time after x ms.
};

enum class TimerResult
{
    Ok,
    Delete
};

enum class ScrollDirection
{
    Up,
    Down
};

template<typename TWidget>
using Callback = std::function<void(TWidget&)>;
template<typename TWidget>
using TimerCallback = std::function<TimerResult(TWidget&)>;

class Widget
{
public:
    Widget() = default;
    virtual ~Widget();

    template<typename TWidget>
    static std::unique_ptr<TWidget> Create()
    {
        return std::make_unique<TWidget>();
    }

    static void CreateAndAddWidget(Widget* widget, GtkWidget* parentWidget);

    void SetClass(const std::string& cssClass);
    void AddClass(const std::string& cssClass);
    void RemoveClass(const std::string& cssClass);
    void SetVerticalTransform(const Transform& transform);
    void SetHorizontalTransform(const Transform& transform);
    void SetTooltip(const std::string& tooltip);

    virtual void Create() = 0;

    void AddChild(std::unique_ptr<Widget>&& widget);
    void RemoveChild(size_t idx);
    void RemoveChild(Widget* widget);

    std::vector<std::unique_ptr<Widget>>& GetWidgets() { return m_Childs; }

    template<typename TWidget>
    void AddTimer(TimerCallback<TWidget>&& callback, uint32_t timeoutMS, TimerDispatchBehaviour dispatch = TimerDispatchBehaviour::ImmediateDispatch)
    {
        struct TimerPayload
        {
            TimerCallback<TWidget> timeoutFn;
            Widget* thisWidget;
        };
        TimerPayload* payload = new TimerPayload();
        payload->thisWidget = this;
        payload->timeoutFn = std::move(callback);
        auto fn = [](void* data) -> int
        {
            TimerPayload* payload = (TimerPayload*)data;
            TimerResult result = payload->timeoutFn(*(TWidget*)payload->thisWidget);
            if (result == TimerResult::Delete)
            {
                delete payload;
                return false;
            }
            return true;
        };
        if (dispatch == TimerDispatchBehaviour::ImmediateDispatch)
        {
            if (fn(payload) == false)
            {
                return;
            }
        }
        g_timeout_add(timeoutMS, +fn, payload);
    }

    GtkWidget* Get() { return m_Widget; };
    const std::vector<std::unique_ptr<Widget>>& GetChilds() const { return m_Childs; };

    void SetVisible(bool visible);

    void SetOnCreate(Callback<Widget>&& onCreate) { m_OnCreate = onCreate; }

protected:
    void PropagateToParent(GdkEvent* event);
    void ApplyPropertiesToWidget();

    GtkWidget* m_Widget = nullptr;

    std::vector<std::unique_ptr<Widget>> m_Childs;

    std::string m_CssClass;
    std::string m_Tooltip;
    Transform m_HorizontalTransform; // X
    Transform m_VerticalTransform;   // Y

    Callback<Widget> m_OnCreate;
};

class Box : public Widget
{
public:
    Box() = default;
    virtual ~Box() = default;

    void SetOrientation(Orientation orientation);
    void SetSpacing(Spacing spacing);

    virtual void Create() override;

private:
    Orientation m_Orientation = Orientation::Horizontal;
    Spacing m_Spacing;
};

class CenterBox : public Widget
{
public:
    void SetOrientation(Orientation orientation);

    virtual void Create() override;

private:
    Orientation m_Orientation = Orientation::Horizontal;
};

class EventBox : public Widget
{
public:
    void SetHoverFn(std::function<void(EventBox&, bool)>&& fn);
    void SetScrollFn(std::function<void(EventBox&, ScrollDirection)>&& fn);
    virtual void Create() override;

private:
    // If two hover events are sent, it needs also two close events for a close.
    // Somehow not doing that causes an issue.
    int32_t m_DiffHoverEvents = 0;
    std::function<void(EventBox&, bool)> m_HoverFn;
    std::function<void(EventBox&, ScrollDirection)> m_ScrollFn;
};

class CairoArea : public Widget
{
public:
    virtual void Create() override;

protected:
    virtual void Draw(cairo_t* cr) = 0;

    Quad GetQuad();
};

class Sensor : public CairoArea
{
public:
    // Goes from 0-1
    void SetValue(double val);
    void SetStyle(SensorStyle style);

private:
    void Draw(cairo_t* cr) override;

    double m_Val;
    SensorStyle m_Style{};
};

class NetworkSensor : public CairoArea
{
public:
    virtual void Create() override;

    void SetLimitUp(Range limit) { limitUp = limit; };
    void SetLimitDown(Range limit) { limitDown = limit; };

    void SetUp(double val);
    void SetDown(double val);

private:
    void Draw(cairo_t* cr) override;

    // These are in percent
    double up, down;

    Range limitUp;
    Range limitDown;

    // What I do here is a little bit gross, but I need a working style context
    // Just manually creating a style context doesn't work for me.
    std::unique_ptr<Box> contextUp;
    std::unique_ptr<Box> contextDown;
};

class Texture : public CairoArea
{
public:
    Texture() = default;
    virtual ~Texture();

    // Non-Owning, ARGB32
    void SetBuf(size_t width, size_t height, uint8_t* buf);

    void ForceHeight(size_t height) { m_ForcedHeight = height; };
    void AddPaddingTop(int32_t topPadding) { m_Padding = topPadding; };

private:
    void Draw(cairo_t* cr) override;

    size_t m_Width;
    size_t m_Height;
    size_t m_ForcedHeight = 0;
    int32_t m_Padding = 0;
    GBytes* m_Bytes;
    GdkPixbuf* m_Pixbuf;
};

class Revealer : public Widget
{
public:
    void SetTransition(Transition transition);

    void SetRevealed(bool revealed);

    virtual void Create() override;

private:
    Transition m_Transition;
};

class Text : public Widget
{
public:
    Text() = default;
    virtual ~Text() = default;

    void SetText(const std::string& text);

    virtual void Create() override;

private:
    std::string m_Text;
};

class Button : public Widget
{
public:
    Button() = default;
    virtual ~Button() = default;

    void SetText(const std::string& text);

    virtual void Create() override;

    void OnClick(Callback<Button>&& callback);

private:
    std::string m_Text;
    Callback<Button> m_OnClick;
};

class Slider : public Widget
{
public:
    Slider() = default;
    virtual ~Slider() = default;

    void SetValue(double value);
    void SetOrientation(Orientation orientation);
    void SetInverted(bool flipped);
    void SetRange(SliderRange range);
    void SetScrollSpeed(double speed);

    void OnValueChange(std::function<void(Slider&, double)>&& callback);

    virtual void Create() override;

private:
    Orientation m_Orientation = Orientation::Horizontal;
    SliderRange m_Range;
    bool m_Inverted = false;
    double m_ScrollSpeed = 5. / 100.; // 5%
    std::function<void(Slider&, double)> m_OnValueChange;
};
