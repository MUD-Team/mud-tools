
#include <Poco/Glob.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>

#include <cstdint>

#include "libimagequant.h"
#include "lodepng.h"

using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::Option;
using Poco::Util::OptionCallback;
using Poco::Util::OptionSet;

// Sample application https://github.com/ISISComputingGroup/poco/blob/master/Util/samples/SampleApp/src/SampleApp.cpp

class MUDImageApp : public Application
{
  public:
    MUDImageApp()
    {
        setUnixOptions(true);
    }

  protected:
    void initialize(Application &self)
    {

        Application::initialize(self);
        // TODO: Add any app initialization code
    }

    void defineOptions(OptionSet &options)
    {
        Application::defineOptions(options);

        options.addOption(Option("help", "h", "display argument help information")
                              .required(false)
                              .repeatable(false)
                              .callback(OptionCallback<MUDImageApp>(this, &MUDImageApp::handleHelp)));

        options.addOption(Option("source", "s", "source directory of png files")
                              .required(true)
                              .repeatable(false)
                              .argument("source")
                              .binding("source"));

        options.addOption(Option("dest", "d", "destination directory for 8 bit png files")
                              .required(true)
                              .repeatable(false)
                              .argument("dest")
                              .binding("dest"));
    }

    void handleHelp(const std::string &name, const std::string &value)
    {
        HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A image processing tool for MUD");
        helpFormatter.format(std::cout);
        stopOptionsProcessing();
        mHelpRequested = true;
    }

    int main(const std::vector<std::string> &args)
    {
        if (mHelpRequested)
        {
            return Application::EXIT_OK;
        }

        process();

        return Application::EXIT_OK;
    }

  private:
    struct InputImage
    {
        std::string path;
        liq_attr   *handle;
        liq_image  *image;
    };

    void process()
    {
        Poco::Path            sourcePath = config().getString("source") + "/*.png";
        std::set<std::string> pngFiles;
        Poco::Glob::glob(sourcePath, pngFiles);

        if (!pngFiles.size())
        {
            std::cout << (std::string("No PNG files to process found ") + sourcePath.toString());
            exit(Application::EXIT_OK);
        }

        liq_attr      *histogram_attr = liq_attr_create();
        liq_histogram *histogram      = liq_histogram_create(histogram_attr);

        for (auto path : pngFiles)
        {
            unsigned int   width, height;
            unsigned char *raw_rgba_pixels;
            unsigned int   status = lodepng_decode32_file(&raw_rgba_pixels, &width, &height, path.c_str());
            if (status)
            {
                std::cerr << "Error loading file: " + path;
                exit(Application::EXIT_IOERR);
            }

            liq_attr  *handle      = liq_attr_create();
            liq_image *input_image = liq_image_create_rgba(handle, raw_rgba_pixels, width, height, 0);
            liq_histogram_add_image(histogram, histogram_attr, input_image);

            mInputImages.push_back({path, handle, input_image});
        }

        liq_result *res;
        liq_error   err = liq_histogram_quantize(histogram, histogram_attr, &res);

        if (err != LIQ_OK)
        {
            std::cerr << "Error creating histogram";
            exit(-1);
        }

        const liq_palette *palette = liq_get_palette(res);

        for (auto image : mInputImages)
        {
            int32_t        width       = liq_image_get_width(image.image);
            int32_t        height      = liq_image_get_height(image.image);
            uint32_t       buffer_size = width * height;
            const uint8_t *buffer      = (uint8_t *)malloc(buffer_size);

            liq_write_remapped_image(res, image.image, (void *)buffer, buffer_size);

            LodePNGState state;
            lodepng_state_init(&state);
            state.info_raw.colortype       = LCT_PALETTE;
            state.info_raw.bitdepth        = 8;
            state.info_png.color.colortype = LCT_PALETTE;
            state.info_png.color.bitdepth  = 8;

            for (int i = 0; i < palette->count; i++)
            {
                lodepng_palette_add(&state.info_png.color, palette->entries[i].r, palette->entries[i].g,
                                    palette->entries[i].b, palette->entries[i].a);
                lodepng_palette_add(&state.info_raw, palette->entries[i].r, palette->entries[i].g,
                                    palette->entries[i].b, palette->entries[i].a);
            }

            unsigned char *output_file_data;
            size_t         output_file_size;
            unsigned int   out_status =
                lodepng_encode(&output_file_data, &output_file_size, buffer, width, height, &state);
            if (out_status)
            {
                std::cerr << (std::string("Can't encode image: ") + lodepng_error_text(out_status));
                exit(-1);
            }

            std::string output_png_file_path = config().getString("dest") + "/" + Poco::Path(image.path).getFileName();
            FILE       *fp                   = fopen(output_png_file_path.c_str(), "wb");
            if (!fp)
            {
                std::cerr << "Unable to write to " + output_png_file_path;
                exit(-1);
            }
            fwrite(output_file_data, 1, output_file_size, fp);
            fclose(fp);

            free((void *)buffer);
            liq_image_destroy(image.image);
            liq_attr_destroy(image.handle);
            lodepng_state_cleanup(&state);
        }

        liq_result_destroy(res);
        liq_histogram_destroy(histogram);
        liq_attr_destroy(histogram_attr);
    }

    std::vector<InputImage> mInputImages;

    std::string mErrorMessage;

    bool mHelpRequested = false;
};

POCO_APP_MAIN(MUDImageApp)