//
// Created by cpasjuste on 05/01/18.
//

#ifndef C2DUI_LISTBOX_H
#define C2DUI_LISTBOX_H

namespace pemu {

    class UIListBoxLine : public c2d::RectangleShape {

    public:

        UIListBoxLine(const c2d::FloatRect &rect, const std::string &str, c2d::Font *font,
                      unsigned int fontSize, const c2d::Vector2f &scaling,
                      c2d::Texture *icon = nullptr, bool use_icons = false);

        ~UIListBoxLine() override;

        void setSize(const c2d::Vector2f &size) override;

        void setSize(float width, float height) override;

        void setString(const std::string &string);

        // 跑马灯：仅当前高亮行启用；按码点滑窗切子串，不做像素平移（不依赖控件裁剪）
        void setMarqueeEnabled(bool enable);

        void updateMarquee();

        void setColor(const c2d::Color &color);

        void setIcon(c2d::Texture *icon);

        c2d::Text *getText();

    private:

        RectangleShape *iconRect = nullptr;
        c2d::Texture *icon = nullptr;
        c2d::Text *text = nullptr;
        bool use_icons = false;
        // 跑马灯状态（不改变本文件原有的截断/显示逻辑）
        float marqueeWidth = 0;           // 文本可用宽度，取自构造函数里 setSizeMax 的同一表达式
        std::string fullString;           // 完整名称，滚动窗口从这里切
        std::vector<float> charOffsets;   // 每个码点的左边界 x，共 count + 1 项（末项为整串宽度）
        size_t marqueeStart = 0;          // 当前窗口起始码点
        int marqueeHoldTicks = 0;         // 到达尾部后的停顿计数
        bool marqueeEnabled = false;
        c2d::C2DClock marqueeClock;
    };

    class UIListBox : public c2d::RectangleShape {

    public:

        UIListBox(UiMain *ui, c2d::Font *font, int fontSize, const c2d::FloatRect &rect,
                  const std::vector<ss_api::Game> &games);

        ~UIListBox() override;

        void setGames(const std::vector<ss_api::Game> &games);

        // 跑马灯驱动：只滚动当前高亮行，其余行保持原有显示
        void updateMarquee();

        void setSelection(int index);

        void up();

        void down();

        void setSize(const c2d::Vector2f &size) override;

        void setSize(float width, float height) override;

        void setTextOutlineColor(const c2d::Color &color);

        void setTextOutlineThickness(float thickness);

        void setHighlightEnabled(bool enable);

        void setHighlightUseFileColor(bool enable);

        RectangleShape *getHighlight();

        int getMaxLines();

        std::vector<UIListBoxLine *> getLines();

        std::vector<ss_api::Game> getGames();

        ss_api::Game getSelection();

        int getIndex();

        c2d::Color colorMissing;
        c2d::Color colorAvailable;

    private:

        void init(c2d::Font *font, int fontSize);

        void updateLines();

        UiMain *ui = nullptr;
        std::vector<ss_api::Game> games;
        std::vector<UIListBoxLine *> lines;
        RectangleShape *highlight;
        float line_height;
        int max_lines;
        int file_index = 0;
        int highlight_index = 0;
        bool use_highlight = true;
        bool highlight_use_files_color = false;
    };
}

#endif //C2DUI_LISTBOX_H
