#include "Widget.h"
#include "Common.h"
#include "CSS.h"

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
    if (m_Widget && m_CssClass != cssClass)
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
        gtk_container_remove((GtkContainer*)m_Widget, child.m_Widget);
        child.m_Widget = nullptr;
    }
    m_Childs.erase(m_Childs.begin() + idx);
}
void Widget::RemoveChild(Widget* widget)
{
    auto it = std::find_if(m_Childs.begin(), m_Childs.end(),
                           [&](std::unique_ptr<Widget>& other)
                           {
                               return other.get() == widget;
                           });
    if (it != m_Childs.end())
    {
        if (m_Widget)
        {
            gtk_container_remove((GtkContainer*)m_Widget, it->get()->m_Widget);
            it->get()->m_Widget = nullptr;
        }
        m_Childs.erase(it);
    }
    else
    {
        LOG("Invalid child!");
    }
}

void Widget::SetVisible(bool visible)
{
    gtk_widget_set_visible(m_Widget, visible);
}

void Widget::PropagateToParent(GdkEvent* event)
{
    gtk_propagate_event(gtk_widget_get_parent(m_Widget), event);
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
    gtk_widget_set_margin_start(m_Widget, m_HorizontalTransform.marginBefore);
    gtk_widget_set_margin_end(m_Widget, m_HorizontalTransform.marginAfter);
    gtk_widget_set_margin_top(m_Widget, m_VerticalTransform.marginBefore);
    gtk_widget_set_margin_bottom(m_Widget, m_VerticalTransform.marginAfter);

    if (m_OnCreate)
        m_OnCreate(*this);
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

void EventBox::SetHoverFn(std::function<void(EventBox&, bool)>&& fn)
{
    m_HoverFn = fn;
}

void EventBox::SetScrollFn(std::function<void(EventBox&, ScrollDirection)>&& fn)
{
    m_ScrollFn = fn;
}

void EventBox::Create()
{
    m_Widget = gtk_event_box_new();
    gtk_event_box_set_above_child((GtkEventBox*)m_Widget, false);
    auto enter = [](GtkWidget*, GdkEventCrossing*, gpointer data) -> gboolean
    {
        EventBox* box = (EventBox*)data;
        if (box->m_HoverFn && box->m_DiffHoverEvents <= 0)
        {
            box->m_HoverFn(*box, true);
            box->m_DiffHoverEvents = 0;
        }
        box->m_DiffHoverEvents++;
        return false;
    };
    auto leave = [](GtkWidget*, GdkEventCrossing*, void* data) -> gboolean
    {
        EventBox* box = (EventBox*)data;
        box->m_DiffHoverEvents--;
        if (box->m_HoverFn && box->m_DiffHoverEvents <= 0)
        {
            box->m_HoverFn(*box, false);
            box->m_DiffHoverEvents = 0;
        }
        return false;
    };
    // I am so done with the GTK docs. The docs clearly say GdkEventScroll and not GdkEventScroll*, but GdkEventScroll* is passed
    auto scroll = [](GtkWidget*, GdkEventScroll* event, void* data) -> gboolean
    {
        EventBox* box = (EventBox*)data;
        if (box->m_ScrollFn)
        {
            if (event->direction == GDK_SCROLL_DOWN)
            {
                box->m_ScrollFn(*box, ScrollDirection::Down);
            }
            else if (event->direction == GDK_SCROLL_UP)
            {
                box->m_ScrollFn(*box, ScrollDirection::Up);
            }
        }
        return false;
    };
    gtk_widget_set_events(m_Widget, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK);
    g_signal_connect(m_Widget, "enter-notify-event", G_CALLBACK(+enter), this);
    g_signal_connect(m_Widget, "leave-notify-event", G_CALLBACK(+leave), this);
    g_signal_connect(m_Widget, "scroll-event", G_CALLBACK(+scroll), this);

    ApplyPropertiesToWidget();
}

void CairoArea::Create()
{
    m_Widget = gtk_drawing_area_new();
    auto drawFn = [](GtkWidget*, cairo_t* c, void* data) -> gboolean
    {
        CairoArea* area = (CairoArea*)data;
        area->Draw(c);
        return false;
    };

    g_signal_connect(m_Widget, "draw", G_CALLBACK(+drawFn), this);

    ApplyPropertiesToWidget();
}

Quad CairoArea::GetQuad()
{
    GtkAllocation dim;
    gtk_widget_get_allocation(m_Widget, &dim);
    Quad q;
    if (dim.height >= dim.width)
    {
        // Height greater than width; Fill in x and add margin at the top and bottom
        q.size = dim.width;
        q.x = 0;
        q.y = ((double)dim.height - (double)dim.width) / 2;
    }
    else if (dim.height < dim.width)
    {
        // Height greater than width; Fill in y and add margin at the sides
        q.size = dim.height;
        q.y = 0;
        q.x = ((double)dim.width - (double)dim.height) / 2;
    }
    return q;
}

void Sensor::SetValue(double val)
{
    if (val != m_Val)
    {
        m_Val = val;
        if (m_Widget)
        {
            gtk_widget_queue_draw(m_Widget);
        }
    }
}

void Sensor::SetStyle(SensorStyle style)
{
    m_Style = style;
}

void Sensor::Draw(cairo_t* cr)
{
    Quad q = GetQuad();

    double xCenter = q.x + q.size / 2;
    double yCenter = q.y + q.size / 2;
    double radius = (q.size / 2) - (m_Style.strokeWidth / 2);

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

static std::string NetworkSensorPercentToCSS(double percent)
{
    if (percent <= 0.)
    {
        return "under";
    }
    else if (percent <= 0.25)
    {
        return "low";
    }
    else if (percent <= 0.50)
    {
        return "mid-low";
    }
    else if (percent <= 0.75)
    {
        return "mid-high";
    }
    else if (percent <= 1.)
    {
        return "high";
    }
    else
    {
        return "over";
    }
}

static double NetworkSensorRateToPercent(double rate, Range range)
{
    return (rate - range.min) / (range.max - range.min);
}

void NetworkSensor::Create()
{
    CairoArea::Create();

    // Add virtual children for style context(I know, it is really gross)
    contextUp = Widget::Create<Box>();
    contextUp->SetSpacing({0, true});
    contextUp->SetClass("network-up-under");
    contextUp->SetHorizontalTransform({0, false, Alignment::Fill});
    contextUp->Create();

    contextDown = Widget::Create<Box>();
    contextDown->SetSpacing({0, true});
    contextDown->SetClass("network-down-under");
    contextDown->SetHorizontalTransform({0, false, Alignment::Fill});
    contextDown->Create();
}

void NetworkSensor::SetUp(double val)
{
    if (!contextUp)
    {
        return;
    }

    up = NetworkSensorRateToPercent(val, limitUp);

    // Add css class
    std::string newClass = NetworkSensorPercentToCSS(up);
    contextUp->SetClass("network-up-" + newClass);

    // Schedule redraw
    if (m_Widget)
    {
        gtk_widget_queue_draw(m_Widget);
    }
}

void NetworkSensor::SetDown(double val)
{
    if (!contextDown)
    {
        return;
    }

    down = NetworkSensorRateToPercent(val, limitDown);

    // Add css class
    std::string newClass = NetworkSensorPercentToCSS(down);
    contextDown->SetClass("network-down-" + newClass);

    // Schedule redraw
    if (m_Widget)
    {
        gtk_widget_queue_draw(m_Widget);
    }
}

void NetworkSensor::Draw(cairo_t* cr)
{
    constexpr double epsilon = 1;

    Quad q = GetQuad();
    auto virtToPx = [&](double virtPx)
    {
        return q.size * (virtPx / 24.f);
    };

    GdkRGBA* colUp;
    GdkRGBA* colDown;
    gtk_style_context_get(gtk_widget_get_style_context(contextUp->Get()), GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_COLOR, &colUp, NULL);
    gtk_style_context_get(gtk_widget_get_style_context(contextDown->Get()), GTK_STATE_FLAG_NORMAL, GTK_STYLE_PROPERTY_COLOR, &colDown, NULL);

    // Rotate around center of Quad
    cairo_translate(cr, q.x + q.size / 2, q.y + q.size / 2);
    cairo_rotate(cr, m_Angle * M_PI / 180.0);
    cairo_translate(cr, -(q.x + q.size / 2), -(q.y + q.size / 2));

    // Upload
    cairo_set_source_rgb(cr, colUp->red, colUp->green, colUp->blue);

    // Triangle
    cairo_move_to(cr, q.x + virtToPx(6), q.y + virtToPx(0));   // Top mid
    cairo_line_to(cr, q.x + virtToPx(0), q.y + virtToPx(10));  // Left bottom
    cairo_line_to(cr, q.x + virtToPx(12), q.y + virtToPx(10)); // Right bottom
    cairo_close_path(cr);
    cairo_fill(cr);

    // Rectangle
    // Go a bit above, to avoid gaps between tri and quad
    cairo_rectangle(cr, q.x + virtToPx(4), q.y + virtToPx(10 - epsilon), virtToPx(4), virtToPx(12 + epsilon));
    cairo_fill(cr);

    // Download
    cairo_set_source_rgb(cr, colDown->red, colDown->green, colDown->blue);

    // Triangle
    cairo_move_to(cr, q.x + virtToPx(18), q.y + virtToPx(24)); // Bottom mid
    cairo_line_to(cr, q.x + virtToPx(12), q.y + virtToPx(14)); // Left top
    cairo_line_to(cr, q.x + virtToPx(24), q.y + virtToPx(14)); // Right top
    cairo_close_path(cr);
    cairo_fill(cr);

    // Rectangle
    // Go a bit below, to avoid gaps between tri and quad
    cairo_rectangle(cr, q.x + virtToPx(16), q.y + virtToPx(2), virtToPx(4), virtToPx(12 + epsilon));
    cairo_fill(cr);

    gdk_rgba_free(colUp);
    gdk_rgba_free(colDown);
}

Texture::~Texture()
{
    if (m_Pixbuf)
        g_free(m_Pixbuf);
    if (m_Bytes)
        g_free(m_Bytes);
}

void Texture::SetBuf(size_t width, size_t height, uint8_t* buf)
{
    m_Width = width;
    m_Height = height;
    m_Bytes = g_bytes_new(buf, m_Width * m_Height * 4);
    m_Pixbuf = gdk_pixbuf_new_from_bytes((GBytes*)m_Bytes, GDK_COLORSPACE_RGB, true, 8, m_Width, m_Height, m_Width * 4);
}

void Texture::Draw(cairo_t* cr)
{
    GtkAllocation dim;
    gtk_widget_get_allocation(m_Widget, &dim);

    double height = m_ForcedHeight != 0 ? m_ForcedHeight : dim.height;
    double scale = (double)height / (double)m_Height;
    double width = (double)m_Width * scale;

    gtk_widget_set_size_request(m_Widget, width + 2, height);
    cairo_scale(cr, scale, scale);
    cairo_rectangle(cr, (dim.width - width) / 2.0, m_Padding + (dim.height - height) / 2.0, m_Width, m_Height);
    gdk_cairo_set_source_pixbuf(cr, m_Pixbuf, (dim.width - width) / 2.0, m_Padding + (dim.height - height) / 2.0);
    cairo_fill(cr);
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
    if (m_Widget && text != m_Text)
    {
        gtk_label_set_text((GtkLabel*)m_Widget, text.c_str());
    }
    m_Text = text;
}

void Text::SetAngle(double angle)
{
    if (m_Widget && angle != m_Angle)
    {
        gtk_label_set_angle((GtkLabel*)m_Widget, angle);
    }
    m_Angle = angle;
}

void Text::Create()
{
    m_Widget = gtk_label_new(m_Text.c_str());
    gtk_label_set_angle((GtkLabel*)m_Widget, m_Angle);
    ApplyPropertiesToWidget();
}

void Button::Create()
{
    m_Widget = gtk_button_new_with_label(m_Text.c_str());
    auto clickFn = [](UNUSED GtkButton* gtkButton, void* data) -> gboolean
    {
        Button* button = (Button*)data;
        if (button->m_OnClick)
            button->m_OnClick(*button);
        return GDK_EVENT_STOP;
    };
    g_signal_connect(m_Widget, "clicked", G_CALLBACK(+clickFn), this);
    gtk_container_foreach((GtkContainer*)m_Widget,
                          [](GtkWidget* child, void* userData)
                          {
                              if (GTK_IS_LABEL(child))
                              {
                                  gtk_label_set_angle((GtkLabel*)child, *(double*)userData);
                              }
                          },
                          &m_Angle);
    ApplyPropertiesToWidget();
}

void Button::SetText(const std::string& text)
{
    if (m_Widget && text != m_Text)
    {
        gtk_button_set_label((GtkButton*)m_Widget, text.c_str());
    }
    m_Text = text;
}

void Button::SetAngle(double angle)
{
    if (m_Widget && angle != m_Angle)
    {
        gtk_container_foreach((GtkContainer*)m_Widget,
                              [](GtkWidget* child, void* userData)
                              {
                                  if (GTK_IS_LABEL(child))
                                  {
                                      gtk_label_set_angle((GtkLabel*)child, *(double*)userData);
                                  }
                              },
                              &angle);
    }
    m_Angle = angle;
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

void Slider::SetScrollSpeed(double speed)
{
    m_ScrollSpeed = speed;
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

    auto scroll = [](GtkWidget*, GdkEventScroll* event, void* data) -> gboolean
    {
        Slider* slider = (Slider*)data;
        double value = gtk_range_get_value((GtkRange*)slider->m_Widget);
        // Range generates a 'smooth' event.
        if (event->delta_y >= 0)
        {
            value -= slider->m_ScrollSpeed;
            slider->SetValue(value);
            if (slider->m_OnValueChange)
                slider->m_OnValueChange(*slider, value);
        }
        else if (event->delta_y <= 0)
        {
            value += slider->m_ScrollSpeed;
            slider->SetValue(value);
            if (slider->m_OnValueChange)
                slider->m_OnValueChange(*slider, value);
        }
        return GDK_EVENT_STOP;
    };
    g_signal_connect(m_Widget, "scroll-event", G_CALLBACK(+scroll), this);

    // Propagate events to any parent eventboxes
    auto propagate = [](GtkWidget*, GdkEventCrossing* data, gpointer widget) -> gboolean
    {
        Slider* slider = (Slider*)widget;
        // Seems to be necessary. For revealers to work properly, we need to notify it of the event through propagation.
        // Automatic propagation with GDK_EVENT_PROPAGATE doesn't work for some reason
        slider->PropagateToParent((GdkEvent*)data);
        return GDK_EVENT_PROPAGATE;
    };
    gtk_widget_set_events(m_Widget, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK);
    g_signal_connect(m_Widget, "enter-notify-event", G_CALLBACK(+propagate), this);
    g_signal_connect(m_Widget, "leave-notify-event", G_CALLBACK(+propagate), this);
    ApplyPropertiesToWidget();
}
