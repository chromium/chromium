// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/vector_example.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

namespace {

class VectorIconGallery : public View, public TextfieldController {
  METADATA_HEADER(VectorIconGallery, View)

 public:
  VectorIconGallery() {
    size_input_ = AddChildView(std::make_unique<Textfield>());
    color_input_ = AddChildView(std::make_unique<Textfield>());

    image_view_container_ = AddChildView(std::make_unique<views::View>());

    BoxLayout* box = SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kVertical, gfx::Insets(10), 10));
    box->SetFlexForView(image_view_container_, 1);

    base::FilePath test_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_dir);
    std::u16string base_path = test_dir.AsUTF16Unsafe();
    std::vector<std::u16string> icon_dir = {
        base::FilePath(test_dir.AppendASCII("ash")
                           .AppendASCII("resources")
                           .AppendASCII("vector_icons"))
            .AsUTF16Unsafe(),
        base::FilePath(
            test_dir.AppendASCII("chrome").AppendASCII("app").AppendASCII(
                "vector_icons"))
            .AsUTF16Unsafe(),
        base::FilePath(test_dir.AppendASCII("chromeos")
                           .AppendASCII("ui")
                           .AppendASCII("vector_icons"))
            .AsUTF16Unsafe(),
        base::FilePath(
            test_dir.AppendASCII("components").AppendASCII("vector_icons"))
            .AsUTF16Unsafe(),
        base::FilePath(test_dir.AppendASCII("components")
                           .AppendASCII("omnibox")
                           .AppendASCII("browser")
                           .AppendASCII("vector_icons"))
            .AsUTF16Unsafe(),
        base::FilePath(
            test_dir.AppendASCII("ui").AppendASCII("views").AppendASCII(
                "vector_icons"))
            .AsUTF16Unsafe(),
    };
    auto editable_combobox = std::make_unique<views::EditableCombobox>();
    editable_combobox->SetModel(std::make_unique<ui::SimpleComboboxModel>(
        std::vector<ui::SimpleComboboxModel::Item>(icon_dir.begin(),
                                                   icon_dir.end())));
    editable_combobox->SetPlaceholderText(
        GetStringUTF16(IDS_VECTOR_FILE_SELECT_LABEL));
    editable_combobox->GetViewAccessibility().SetName(u"Editable Combobox");

    auto file_container = std::make_unique<View>();
    BoxLayout* file_box =
        file_container->SetLayoutManager(std::make_unique<BoxLayout>(
            BoxLayout::Orientation::kHorizontal, gfx::Insets(10), 10));
    file_chooser_combobox_ =
        file_container->AddChildView(std::move(editable_combobox));

    file_go_button_ =
        file_container->AddChildView(std::make_unique<MdTextButton>(
            base::BindRepeating(&VectorIconGallery::FileGoButtonPressed,
                                base::Unretained(this)),
            GetStringUTF16(IDS_VECTOR_RENDER_LABEL)));
    file_box->SetFlexForView(file_chooser_combobox_, 1);
    AddChildView(std::move(file_container));

    size_input_->SetPlaceholderText(
        GetStringUTF16(IDS_VECTOR_DIP_SIZE_DESC_LABEL));
    size_input_->set_controller(this);
    color_input_->SetPlaceholderText(
        GetStringUTF16(IDS_VECTOR_COLOR_DESC_LABEL));
    color_input_->set_controller(this);
  }

  VectorIconGallery(const VectorIconGallery&) = delete;
  VectorIconGallery& operator=(const VectorIconGallery&) = delete;

  ~VectorIconGallery() override = default;

  // TextfieldController implementation.
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override {
    if (sender == size_input_) {
      if (base::StringToInt(new_contents, &size_) && (size_ > 0))
        Update();
      else
        size_input_->SetText(std::u16string());

      return;
    }

    DCHECK_EQ(color_input_, sender);
    if (new_contents.size() != 8u)
      return;
    unsigned new_color =
        strtoul(base::UTF16ToASCII(new_contents).c_str(), nullptr, 16);
    if (new_color <= 0xffffffff) {
      color_ = new_color;
      Update();
    }
  }

 private:
  void FileGoButtonPressed() {
#if BUILDFLAG(IS_WIN)
    base::FilePath path(base::UTF16ToWide(file_chooser_combobox_->GetText()));
#else
    base::FilePath path(base::UTF16ToUTF8(file_chooser_combobox_->GetText()));
#endif

    // If there is an extension, then it would not be a folder.
    if (path.Extension().size() == 0) {
      GenerateAllIconInFolder(path);
    } else {
      GenerateSingleIcon(path);
    }
  }

  void GenerateSingleIcon(const base::FilePath& path) {
    image_view_container_->RemoveAllChildViews();
    image_view_ =
        image_view_container_->AddChildView(std::make_unique<ImageView>());
    image_view_->SetBorder(CreateThemedSolidBorder(
        1, ExamplesColorIds::kColorVectorExampleImageBorder));

    auto image_layout =
        std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal);
    image_layout->set_cross_axis_alignment(
        BoxLayout::CrossAxisAlignment::kCenter);
    image_layout->set_main_axis_alignment(
        BoxLayout::MainAxisAlignment::kCenter);
    image_view_container_->SetLayoutManager(std::move(image_layout));

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(path, &contents_);
    contents_ = CleanUpContents(contents_);
    Update();
  }

  void GenerateAllIconInFolder(const base::FilePath& path) {
    image_view_container_->RemoveAllChildViews();

    int nCols = image_view_container_->width() / size_;
    int kColumnWidth = size_;
    views::TableLayout* layout = image_view_container_->SetLayoutManager(
        std::make_unique<views::TableLayout>());
    for (int i = 0; i < nCols; ++i) {
      layout->AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                        TableLayout::kFixedSize,
                        TableLayout::ColumnSize::kFixed, kColumnWidth,
                        kColumnWidth);
    }

    int nRows = image_view_container_->height() / size_;
    for (int i = 0; i < nRows; ++i) {
      layout->AddRows(1, TableLayout::kFixedSize);
    }
    size_t max = nCols * nRows;
    size_t count = 0;

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FileEnumerator file_iter(path, false, base::FileEnumerator::FILES,
                                     FILE_PATH_LITERAL("*.icon"));
      std::vector<base::FilePath> files;
      for (base::FilePath input_file = file_iter.Next(); !input_file.empty();
           input_file = file_iter.Next()) {
        files.push_back(input_file);
      }
      std::sort(files.begin(), files.end());

      for (base::FilePath file : files) {
        if (count++ >= max) {
          break;
        }
        std::string file_content;
        base::ReadFileToString(file, &file_content);

        ImageView* icon_view =
            image_view_container_->AddChildView(std::make_unique<ImageView>());
        icon_view->SetImage(
            ui::ImageModel::FromImageSkia(gfx::CreateVectorIconFromSource(
                CleanUpContents(file_content), size_, color_)));
        icon_view->SetTooltipText(file.BaseName().AsUTF16Unsafe());
        file = file_iter.Next();
      }
    }
    InvalidateLayout();
  }

  void Update() {
    if (!contents_.empty() && image_view_ != nullptr) {
      image_view_->SetImage(ui::ImageModel::FromImageSkia(
          gfx::CreateVectorIconFromSource(contents_, size_, color_)));
    }
    InvalidateLayout();
  }

  std::string CleanUpContents(const std::string& file_content) {
    // Skip over comments.
    // This handles very basic cases of // and /*. More complicated edge
    // cases such as /* /* */ */ are not handled.
    std::string output = file_content;
    for (size_t slashes = output.find("//"); slashes != std::string::npos;
         slashes = output.find("//")) {
      size_t eol = output.find("\n", slashes);
      // Add 1 to erase the \n token at the end of the line.
      output.erase(slashes, eol - slashes + 1);
    }

    for (size_t slashes = output.find("/*"); slashes != std::string::npos;
         slashes = output.find("/*")) {
      size_t eol = output.find("*/", slashes);
      output.erase(slashes, eol - slashes + 2);
    }

    // CreateVectorIconFromSource does not work well if there are multiple icon
    // sizes in the same file. Fetch the first icon source in the file.
    std::string result = output;
    size_t start = 0;
    std::string token = "\n\n";
    size_t end = output.find(token);
    while (end != std::string::npos) {
      std::string section = output.substr(start, end - start);
      if (!section.empty() &&
          section.find_first_not_of("\t\n\v\f\r") != std::string::npos) {
        result = section;
        break;
      }
      start = end;
      end = output.find(token, end + token.length());
    }
    return result;
  }

  // 36dp is one of the natural sizes for MD icons, and corresponds roughly to a
  // 32dp usable area.
  int size_ = 36;
  SkColor color_ = gfx::kPlaceholderColor;

  raw_ptr<ImageView> image_view_;
  raw_ptr<View> image_view_container_;
  raw_ptr<Textfield> size_input_;
  raw_ptr<Textfield> color_input_;
  raw_ptr<EditableCombobox> file_chooser_combobox_;
  raw_ptr<Button> file_go_button_;
  std::string contents_;
};

BEGIN_METADATA(VectorIconGallery)
END_METADATA

}  // namespace

VectorExample::VectorExample()
    : ExampleBase(GetStringUTF8(IDS_VECTOR_SELECT_LABEL).c_str()) {}

VectorExample::~VectorExample() = default;

void VectorExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<VectorIconGallery>());
}

}  // namespace views::examples
