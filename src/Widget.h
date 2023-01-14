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

struct SensorStyle
{
    double start = -90; // 0 = leftmost; -90 = topmost
    double strokeWidth = 4;
};

struct SliderRange
{
    double min, max, step;
};

enum class TimerResult
{
    Ok,
    Delete
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

    std::vector<std::unique_ptr<Widget>>& GetWidgets() {return m_Childs;}

    template<typename TWidget>
    void AddTimer(TimerCallback<TWidget>&& callback, uint32_t timeoutMS)
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
        g_timeout_add(timeoutMS, +fn, payload);
    }

    GtkWidget* Get() { return m_Widget; };
    const std::vector<std::unique_ptr<Widget>>& GetChilds() const { return m_Childs; };

    void SetVisible(bool visible);

protected:
    void ApplyPropertiesToWidget();

    GtkWidget* m_Widget = nullptr;

    std::vector<std::unique_ptr<Widget>> m_Childs;

    std::string m_CssClass;
    std::string m_Tooltip;
    Transform m_HorizontalTransform; // X
    Transform m_VerticalTransform;   // Y
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
    void SetEventFn(std::function<void(EventBox&, bool)>&& fn);
    virtual void Create() override;

private:
    std::function<void(EventBox&, bool)> m_EventFn;
};

class CairoSensor : public Widget
{
public:
    virtual void Create() override;
    
    // Goes from 0-1
    void SetValue(double val);
    void SetStyle(SensorStyle style);

private:
    void Draw(cairo_t* cr);

    double m_Val;
    SensorStyle m_Style{};
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

    void OnValueChange(std::function<void(Slider&, double)>&& callback);

    virtual void Create() override;

private:
    Orientation m_Orientation = Orientation::Horizontal;
    SliderRange m_Range;
    bool m_Inverted = false;
    std::function<void(Slider&, double)> m_OnValueChange;
};
