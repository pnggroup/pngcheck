# frozen_string_literal: true

require 'liquid'

module PngcheckTest
  module Models
    class TestGenerator
      attr_reader :repository, :png_suite

      def initialize(repository, png_suite)
        @repository = repository
        @png_suite = png_suite
      end

      # Generate the test suite C file
      def generate!
        png_files_data = prepare_png_files_data
        statistics = calculate_statistics

        template_path = template_file_path
        template_content = template_path.read
        template = Liquid::Template.parse(template_content)

        template_data = {
          'png_files' => png_files_data,
          'statistics' => statistics,
          'generated_at' => Time.now.strftime('%Y-%m-%d %H:%M:%S')
        }

        content = template.render(template_data)

        @repository.test_suite_file.write(content)

        {
          success: true,
          file: @repository.test_suite_file,
          statistics: statistics
        }
      end

      private

      def prepare_png_files_data
        @png_suite.png_files.map do |png_file|
          category = @png_suite.categorize_png(png_file)
          test_name = sanitize_test_name(png_file.basename('.png').to_s)
          png_filename = png_file.basename.to_s

          {
            'test_name' => test_name,
            'filename' => png_filename,
            'category' => category.to_s
          }
        end
      end

      def sanitize_test_name(filename)
        # Convert filename to valid C identifier
        filename.gsub(/[^a-zA-Z0-9_]/, '_').gsub(/_+/, '_').gsub(/^_|_$/, '')
      end

      def calculate_statistics
        stats = @png_suite.statistics
        {
          valid: stats[:valid],
          invalid: stats[:invalid],
          warning: stats[:warning],
          unknown: stats[:unknown],
          total: @png_suite.png_files.length
        }
      end

      def template_file_path
        # Template is located relative to this file
        Pathname.new(__dir__).parent / 'templates' / 'test_suite.c.liquid'
      end
    end
  end
end
