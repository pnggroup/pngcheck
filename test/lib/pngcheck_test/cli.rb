# frozen_string_literal: true

require 'thor'
require 'paint'
require_relative 'version'
require_relative 'models/repository'
require_relative 'models/pngcheck'
require_relative 'models/png_suite'
require_relative 'models/test_generator'

module PngcheckTest
  class CLI < Thor
    include Thor::Actions

    def self.exit_on_failure?
      true
    end

    desc 'expectations', 'Generate expected output files for PNG test suite'
    option :force, type: :boolean, default: false, desc: 'Force regeneration of existing expectations'
    def expectations
      setup_models
      ensure_pngcheck!

      unless @png_suite.available?
        say Paint["❌ Test PNG files missing from #{@png_suite.png_dir}", :red]
        exit 1
      end

      say Paint["🔧 Generating expected outputs...", :cyan, :bold]

      results = @png_suite.generate_expectations!(@pngcheck, force: options[:force])

      say Paint["✅ Processed #{results[:processed]} files", :green]
      say Paint["✅ Set expectations for #{results[:success]} successes", :green] if results[:success] > 0
      say Paint["❌ Set expectations for #{results[:errors]} errors", :red] if results[:errors] > 0

      if options[:verbose]
        results[:files].each do |file_result|
          status_color = case file_result[:status]
                        when :success then :green
                        when :error then :red
                        when :skipped then :yellow
                        end
          say "  #{Paint[file_result[:file], status_color]} - #{file_result[:message]}"
        end
      end
    rescue => e
      handle_error("Failed to generate expectations", e)
    end

    desc 'generate', 'Generate Unity C test suite file'
    def generate
      setup_models

      unless @png_suite.available?
        say Paint["❌ Test PNG files missing from #{@png_suite.png_dir}", :red]
        exit 1
      end

      unless @png_suite.expectation_files.any?
        say Paint["📝 No expectations found. Generating first...", :yellow]
        invoke :expectations
      end

      say Paint["🏗️  Generating Unity test suite...", :cyan, :bold]

      result = @test_generator.generate!

      if result[:success]
        say Paint["✅ Generated test suite: #{result[:file].relative_path_from(@repository.root_path)}", :green]

        stats = result[:statistics]
        say Paint["📊 Test Statistics:", :blue, :bold]
        say "  #{Paint['Valid PNGs:', :green]} #{stats[:valid]}"
        say "  #{Paint['Invalid PNGs:', :red]} #{stats[:invalid]}"
        say "  #{Paint['Warning PNGs:', :yellow]} #{stats[:warning]}"
        say "  #{Paint['Unknown PNGs:', :gray]} #{stats[:unknown]}" if stats[:unknown] > 0
        say "  #{Paint['Total Tests:', :blue, :bold]} #{stats[:total]}"
      else
        say Paint["❌ Failed to generate test suite", :red]
        exit 1
      end
    rescue => e
      handle_error("Failed to generate test suite", e)
    end

    desc 'setup', 'Complete setup: generate expectations, and create test suite'
    option :force, type: :boolean, default: false, desc: 'Force regeneration of all files'
    def setup
      say Paint["🚀 Setting up complete PNG test suite...", :cyan, :bold]

      invoke :expectations, [], force: options[:force]
      invoke :generate

      say Paint["🎉 Setup complete! You can now run tests with:", :green, :bold]
      say Paint["   bundle exec ceedling test:all", :blue]
    rescue => e
      handle_error("Setup failed", e)
    end

    desc 'status', 'Show current status of test suite components'
    def status
      setup_models

      say Paint["📋 PNG Test Suite Status", :cyan, :bold]
      say

      # Repository info
      say Paint["📁 Repository:", :blue, :bold]
      say "  Root: #{@repository.root_path}"
      say "  Valid: #{@repository.valid? ? Paint['✅ Yes', :green] : Paint['❌ No', :red]}"
      say

      # PngCheck executable
      say Paint["🔧 PngCheck Executable:", :blue, :bold]
      if @pngcheck && @pngcheck.valid?
        say "  Path: #{Paint[@pngcheck.executable_path, :green]}"
        version = @pngcheck.version
        say "  Version: #{version}" if version
      else
        say Paint["  ❌ Not found or invalid", :red]
        say "  Run 'make' to build pngcheck or set PNGCHECK_EXECUTABLE"
      end
      say

      # PNG Suite files
      say Paint["🖼️  PNG Suite Files:", :blue, :bold]
      if @png_suite.available?
        stats = @png_suite.statistics
        say "  Available: #{Paint['✅ Yes', :green]} (#{stats[:total_files]} files)"
        say "  Expectations: #{stats[:expectations_generated]} generated"

        if stats[:total_files] > 0
          say "  Categories:"
          say "    Valid: #{Paint[stats[:valid], :green]}"
          say "    Invalid: #{Paint[stats[:invalid], :red]}"
          say "    Warning: #{Paint[stats[:warning], :yellow]}"
          say "    Unknown: #{Paint[stats[:unknown], :gray]}" if stats[:unknown] > 0
        end
      else
        say Paint["  ❌ Test PNG files missing from #{@png_suite.png_dir}", :red]
      end
      say

      # Test suite file
      say Paint["🧪 Test Suite File:", :blue, :bold]
      if @repository.test_suite_file.exist?
        say "  Generated: #{Paint['✅ Yes', :green]}"
        say "  File: #{@repository.test_suite_file.relative_path_from(@repository.root_path)}"
        say "  Size: #{@repository.test_suite_file.size} bytes"
        say "  Modified: #{@repository.test_suite_file.mtime.strftime('%Y-%m-%d %H:%M:%S')}"
      else
        say Paint["  ❌ Not generated", :red]
        say "  Run 'pngcheck-test generate' to create test suite"
      end
    rescue => e
      handle_error("Failed to get status", e)
    end

    desc 'version', 'Show version information'
    def version
      say Paint["pngcheck-test version #{VERSION}", :blue, :bold]
    end

    private

    def setup_models
      @repository = Models::Repository.new
      @png_suite = Models::PngSuite.new(@repository)
      @test_generator = Models::TestGenerator.new(@repository, @png_suite)

      # Try to initialize pngcheck, but don't fail if it's not found for status command
      begin
        @pngcheck = Models::PngCheck.new(@repository)
      rescue Models::PngCheck::ExecutableNotFoundError => e
        @pngcheck = nil
        @pngcheck_error = e.message
      end
    rescue Models::Repository::RepositoryNotFoundError => e
      say Paint["❌ #{e.message}", :red]
      say Paint["Please run this command from within the pngcheck repository.", :yellow]
      exit 1
    end

    def ensure_pngcheck!
      return if @pngcheck

      say Paint["❌ #{@pngcheck_error}", :red]
      exit 1
    end

    def handle_error(message, error)
      say Paint["❌ #{message}: #{error.message}", :red]
      say Paint["💡 Run with --verbose for more details", :yellow] unless options[:verbose]

      if options[:verbose]
        say Paint["🔍 Error details:", :gray]
        say error.backtrace.join("\n")
      end

      exit 1
    end
  end
end
