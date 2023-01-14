#include "Widget.h"
#include "Common.h"

#include <cmath>

// TODO: Currently setters only work pre-create. Make them react to changes after creation!

namespace Utils
{
    GtkAlign ToGtkAlign(Alignment align)
    {
        switch (align)
        {
        case Alignment::Left: return GTK_ALIGN_START;
        case Alignment::Right: return GTK_ALIGN_END;
        case Alignment::Center: return GTK_ALIGN_CENTER;
        case Alignment::Fill: return GTK_ALIGN_FILL;
        }
    }
    GtkOrientation ToGtkOrientation(Orientation orientation)
    {
        switch (orientation)
        {
        case Orientation::Vertical: return GTK_ORIENTATION_VERTICAL;
        case Orientation::Horizontal: return GTK_ORIENTATION_HORIZONTAL;
        }
    }
    GtkRevealerTransitionType ToGtkRevealerTransitionType(TransitionType transition)
    {
        switch (transition)
        {
        case TransitionType::Fade: return GTK_REVEALER_TRANSITION_TYPE_CROSSFADE;
        case TransitionType::SlideLeft: return GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
        case TransitionType::SlideRight: return GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT;
        case TransitionType::SlideDown: return GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN;
        case TransitionType::SlideUp: return GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP;
        }
    }
}

Widget::~Widget()
{
    m_Childs.clear();
    LOG("Destroy widget");
    gtk_widget_destroy(m_Widget);
}

void Widget::CreateAndAddWidget(Widget* widget, GtkWidget* parentWidget)
{
    // Create this widget
    widget->Create();
    // Add
    gtk_container_add((GtkContainer*)parentWidget, widget->Get());

    gtk_widget_show(widget->m_Widget);

    for (auto& child : widget->GetChilds())
    {
        CreateAndAddWidget(child.get(), widget->Get());
    }
}

void Widget::SetClass(const std::string& cssClass)
{
    if (m_Widget)
    {
        auto style = gtk_widget_get_style_context(m_Widget);
        gtk_style_context_remove_class(style, m_CssClass.c_str());
        gtk_style_context_add_class(style, cssClass.c_str());
    }
    m_CssClass = cssClass;
}
void Widget::AddClass(const std::string& cssClass)
{
    if (m_Widget)
    {
        auto style = gtk_widget_get_style_context(m_Widget);
        gtk_style_context_add_class(style, cssClass.c_str());
    }
}
void Widget::RemoveClass(const std::string& cssClass)
{
    if (m_Widget)
    {
        auto style = gtk_widget_get_style_context(m_Widget);
        gtk_style_context_remove_class(style, cssClass.c_str());
    }
}

void Widget::SetVerticalTransform(const Transform& transform)
{
    m_VerticalTransform = transform;
}

void Widget::SetHorizontalTransform(const Transform& transform)
{
    m_HorizontalTransform = transform;
}

void Widget::SetTooltip(const std::string& tooltip)
{
    if (m_Widget)
    {
        gtk_widget_set_tooltip_text(m_Widget, tooltip.c_str());
    }
    m_Tooltip = tooltip;
}

void Widget::AddChild(std::unique_ptr<Widget>&& widget)
{
    if (m_Widget)
    {
        CreateAndAddWidget(widget.get(), m_Widget);
    }
    m_Childs.push_back(std::move(widget));
}

void Widget::RemoveChild(size_t idx)
{
    ASSERT(idx < m_Childs.size(), "RemoveChild: Invalid index");
    if (m_Widget)
    {
        auto& child = *m_Childs[idx];
        gtk_container_remove((GtkContainer*)child.m_Widget, m_Widget);
    }
    m_Childs.erase(m_Childs.begin() + idx);
}

void Widget::SetVisible(bool visible)
{
    gtk_widget_set_visible(m_Widget, visible);
}

void Widget::ApplyPropertiesToWidget()
{
    // Apply style
    auto style = gtk_widget_get_style_context(m_Widget);
    gtk_style_context_add_class(style, m_CssClass.c_str());

    gtk_widget_set_tooltip_text(m_Widget, m_Tooltip.c_str());

    // Apply transform
    gtk_widget_set_size_request(m_Widget, m_HorizontalTransform.size, m_VerticalTransform.size);
    gtk_widget_set_halign(m_Widget, Utils::ToGtkAlign(m_HorizontalTransform.alignment));
    gtk_widget_set_valign(m_Widget, Utils::ToGtkAlign(m_VerticalTransform.alignment));
    gtk_widget_set_hexpand(m_Widget, m_HorizontalTransform.expand);
    gtk_widget_set_vexpand(m_Widget, m_VerticalTransform.expand);
}

void Box::SetOrientation(Orientation orientation)
{
    m_Orientation = orientation;
}

void Box::SetSpacing(Spacing spacing)
{
    m_Spacing = spacing;
}

void Box::Create()
{
    m_Widget = gtk_box_new(Utils::ToGtkOrientation(m_Orientation), m_Spacing.free);
    gtk_box_set_homogeneous((GtkBox*)m_Widget, m_Spacing.evenly);
    ApplyPropertiesToWidget();
}

void CenterBox::SetOrientation(Orientation orientation)
{
    m_Orientation = orientation;
}

void CenterBox::Create()
{
    ASSERT(m_Childs.size() == 3, "CenterBox needs 3 children!")
    m_Widget = gtk_box_new(Utils::ToGtkOrientation(m_Orientation), 0);

    gtk_box_pack_start((GtkBox*)m_Widget, m_Childs[0]->Get(), true, true, 0);
    gtk_box_set_center_widget((GtkBox*)m_Widget, m_Childs[1]->Get());
    gtk_box_pack_end((GtkBox*)m_Widget, m_Childs[2]->Get(), true, true, 0);

    ApplyPropertiesToWidget();
}

void EventBox::SetEventFn(std::function<void(EventBox&, bool)>&& fn)
{
    m_EventFn = fn;
}

void EventBox::Create()
{
    m_Widget = gtk_event_box_new();
    auto enter = [](GtkWidget*, GdkEventCrossing*, gpointer data) -> gboolean
    {
        EventBox* box = (EventBox*)data;
        box->m_EventFn(*box, true);
        return false;
    };
    auto leave = [](GtkWidget*, GdkEventCrossing*, void* data) -> gboolean
    {
        EventBox* box = (EventBox*)data;
        box->m_EventFn(*box, false);
        return false;
    };
    gtk_widget_set_events(m_Widget, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(m_Widget, "enter-notify-event", G_CALLBACK(+enter), this);
    g_signal_connect(m_Widget, "leave-notify-event", G_CALLBACK(+leave), this);

    ApplyPropertiesToWidget();
}

void CairoSensor::Create()
{
    m_Widget = gtk_drawing_area_new();
    auto drawFn = [](GtkWidget*, cairo_t* c, void* data) -> gboolean
    {
        CairoSensor* sensor = (CairoSensor*)data;
        sensor->Draw(c);
        return false;
    };

    g_signal_connect(m_Widget, "draw", G_CALLBACK(+drawFn), this);

    ApplyPropertiesToWidget();
}

void CairoSensor::SetValue(double val)
{
    m_Val = val;
    if (m_Widget)
    {
        gtk_widget_queue_draw(m_Widget);
    }
}

void CairoSensor::SetStyle(SensorStyle style)
{
    m_Style = style;
}

void CairoSensor::Draw(cairo_t* cr)
{
    GtkAllocation dim;
    gtk_widget_get_allocation(m_Widget, &dim);
    double xStart = 0;
    double yStart = 0;
    double size = dim.width;
    if (dim.height >= dim.width)
    {
        // Height greater than width; Fill in x and add margin at the top and bottom
        size = dim.width;
        xStart = 0;
        yStart = ((double)dim.height - (double)dim.width) / 2;
    }
    else if (dim.width < dim.height)
    {
        // Height greater than width; Fill in y and add margin at the sides
        size = dim.height;
        yStart = 0;
        xStart = ((double)dim.width - (double)dim.height) / 2;
    }

    double xCenter = xStart + size / 2;
    double yCenter = yStart + size / 2;
    double radius = (size / 2) - (m_Style.strokeWidth / 2);

    double beg = m_Style.start * (M_PI / 180);
    double angle = m_Val * 2 * M_PI;

    auto style = gtk_widget_get_style_context(m_Widget);
    GdkRGBA* bgCol;
    GdkRGBA* fgCol;
    gtk_style_context_get(style, GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &bgCol, NULL);
    gtk_style_context_get(style, GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_COLOR, &fgCol, NULL);

    cairo_set_line_width(cr, m_Style.strokeWidth);

    // Outer
    cairo_set_source_rgb(cr, bgCol->red, bgCol->green, bgCol->blue);
    cairo_arc(cr, xCenter, yCenter, radius, 0, 2 * M_PI);
    cairo_stroke(cr);

    // Inner
    cairo_set_source_rgb(cr, fgCol->red, fgCol->green, fgCol->blue);
    cairo_arc(cr, xCenter, yCenter, radius, beg, beg + angle);
    cairo_stroke(cr);

    gdk_rgba_free(bgCol);
    gdk_rgba_free(fgCol);
}

void Revealer::SetTransition(Transition transition)
{
    m_Transition = transition;
}

void Revealer::Create()
{
    m_Widget = gtk_revealer_new();
    gtk_revealer_set_transition_type((GtkRevealer*)m_Widget, Utils::ToGtkRevealerTransitionType(m_Transition.type));
    gtk_revealer_set_transition_duration((GtkRevealer*)m_Widget, m_Transition.durationMS);
    ApplyPropertiesToWidget();
}

void Revealer::SetRevealed(bool revealed)
{
    gtk_revealer_set_reveal_child((GtkRevealer*)m_Widget, revealed);
}

void Text::SetText(const std::string& text)
{
    m_Text = text;
    if (m_Widget)
    {
        gtk_label_set_text((GtkLabel*)m_Widget, m_Text.c_str());
    }
}

void Text::Create()
{
    m_Widget = gtk_label_new(m_Text.c_str());
    ApplyPropertiesToWidget();
}

void Button::Create()
{
    m_Widget = gtk_button_new_with_label(m_Text.c_str());
    auto clickFn = [](UNUSED GtkButton* gtkButton, void* data)
    {
        Button* button = (Button*)data;
        if (button->m_OnClick)
            button->m_OnClick(*button);
    };
    g_signal_connect(m_Widget, "clicked", G_CALLBACK(+clickFn), this);
    ApplyPropertiesToWidget();
}

void Button::SetText(const std::string& text)
{
    m_Text = text;
    if (m_Widget)
    {
        gtk_button_set_label((GtkButton*)m_Widget, m_Text.c_str());
    }
}

void Button::OnClick(Callback<Button>&& callback)
{
    m_OnClick = std::move(callback);
}

void Slider::OnValueChange(std::function<void(Slider&, double)>&& callback)
{
    m_OnValueChange = callback;
}

void Slider::SetRange(SliderRange range)
{
    m_Range = range;
}

void Slider::SetOrientation(Orientation orientation)
{
    m_Orientation = orientation;
}

void Slider::SetValue(double value)
{
    gtk_range_set_value((GtkRange*)m_Widget, value);
}

void Slider::SetInverted(bool inverted)
{
    m_Inverted = inverted;
}

void Slider::Create()
{
    m_Widget = gtk_scale_new_with_range(Utils::ToGtkOrientation(m_Orientation), m_Range.min, m_Range.max, m_Range.step);
    gtk_range_set_inverted((GtkRange*)m_Widget, m_Inverted);
    gtk_scale_set_draw_value((GtkScale*)m_Widget, false);
    auto changedFn = [](GtkScale*, GtkScrollType*, double val, void* data)
    {
        Slider* slider = (Slider*)data;
        if (slider->m_OnValueChange)
            slider->m_OnValueChange(*slider, val);
        return false;
    };
    g_signal_connect(m_Widget, "change-value", G_CALLBACK(+changedFn), this);
    ApplyPropertiesToWidget();
}
