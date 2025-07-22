# frozen_string_literal: true

require 'fileutils'
require 'open3'
require 'net/http'
require 'uri'
require 'zip'

module PngcheckTest
  module Models
    class PngSuite
      PNGSUITE_URL = 'http://www.schaik.com/pngsuite/PngSuite-2017jul19.zip'
      PNGSUITE_FILENAME = 'PngSuite.zip'
      PNGSUITE_DIR = 'PngSuite'

      attr_reader :repository

      def initialize(repository)
        @repository = repository
      end

      # Download and extract PNG suite files
      def download!(force: false)
        if downloaded? && !force
          return { success: true, message: 'PNG suite already downloaded', files_count: png_files.count }
        end

        @repository.ensure_directories!

        begin
          download_and_extract
          cleanup_download_artifacts

          {
            success: true,
            message: 'PNG suite downloaded successfully',
            files_count: png_files.count
          }
        rescue => e
          {
            success: false,
            message: "Failed to download PNG suite: #{e.message}",
            files_count: 0
          }
        end
      end

      # Check if PNG suite is downloaded
      def downloaded?
        @repository.fixtures_path.exist? && png_files.any?
      end

      # Get all PNG files
      def png_files
        return [] unless @repository.fixtures_path.exist?

        @repository.fixtures_path.glob('*.png').sort
      end

      # Generate expectations for all PNG files
      def generate_expectations!(pngcheck, force: false)
        @repository.ensure_directories!

        results = {
          processed: 0,
          success: 0,
          errors: 0,
          files: []
        }

        png_files.each do |png_file|
          expectation_file = expectation_file_for(png_file)

          # Skip if expectation exists and not forcing
          if expectation_file.exist? && !force
            results[:files] << { file: png_file.basename, status: :skipped, message: 'Already exists' }
            next
          end

          result = pngcheck.run(png_file)
          expectation_file.write(result[:output])

          results[:processed] += 1
          if result[:success]
            results[:success] += 1
            results[:files] << { file: png_file.basename, status: :success, message: 'Generated' }
          else
            results[:errors] += 1
            results[:files] << { file: png_file.basename, status: :error, message: "Exit code: #{result[:exit_code]}" }
          end
        end

        results
      end

      # Get expectation files
      def expectation_files
        return [] unless @repository.expectations_path.exist?

        @repository.expectations_path.glob('*.out').sort
      end

      # Get expectation file path for a PNG file
      def expectation_file_for(png_file)
        @repository.expectations_path / "#{png_file.basename}.out"
      end

      # Categorize a PNG file based on its expected output
      def categorize_png(png_file)
        expectation_file = expectation_file_for(png_file)
        return :unknown unless expectation_file.exist?

        content = expectation_file.read

        if content.include?('ERROR:') || content.include?('ERRORS DETECTED')
          :invalid
        elsif content.include?('additional data after IEND chunk') ||
              content.include?('private (invalid?) PLTE chunk') ||
              content.include?('invalid')
          :warning
        else
          :valid
        end
      end

      # Get statistics about PNG files
      def statistics
        files = png_files
        expectations = expectation_files

        categories = files.group_by { |f| categorize_png(f) }

        {
          total_files: files.count,
          expectations_generated: expectations.count,
          valid: categories[:valid]&.count || 0,
          invalid: categories[:invalid]&.count || 0,
          warning: categories[:warning]&.count || 0,
          unknown: categories[:unknown]&.count || 0
        }
      end

      private

      def download_and_extract
        Dir.chdir(@repository.fixtures_path) do
          download_file
          extract_archive
          move_png_files
        end
      end

      def download_file
        uri = URI(PNGSUITE_URL)

        Net::HTTP.start(uri.host, uri.port) do |http|
          request = Net::HTTP::Get.new(uri)
          http.request(request) do |response|
            raise "HTTP #{response.code}: #{response.message}" unless response.code == '200'

            File.open(PNGSUITE_FILENAME, 'wb') do |file|
              response.read_body do |chunk|
                file.write(chunk)
              end
            end
          end
        end
      end

      def extract_archive
        Zip::File.open(PNGSUITE_FILENAME) do |zip_file|
          zip_file.each do |entry|
            entry.extract(entry.name) unless File.exist?(entry.name)
          end
        end
      end

      def move_png_files
        if Dir.exist?(PNGSUITE_DIR)
          Dir.glob("#{PNGSUITE_DIR}/*.png").each do |png_file|
            FileUtils.mv(png_file, '.')
          end
          FileUtils.rm_rf(PNGSUITE_DIR)
        end
      end

      def cleanup_download_artifacts
        artifacts = [@repository.fixtures_path / PNGSUITE_FILENAME]
        artifacts.each { |artifact| artifact.delete if artifact.exist? }
      end
    end
  end
end
