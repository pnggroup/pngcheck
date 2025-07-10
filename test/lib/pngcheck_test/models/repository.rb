# frozen_string_literal: true

require 'pathname'

module PngcheckTest
  module Models
    class Repository
      class RepositoryNotFoundError < StandardError; end

      attr_reader :root_path

      def initialize(start_path = nil)
        # Repository root is 4 levels up from this file: ../../../..
        @root_path = Pathname.new("#{__dir__}/../../../..").expand_path
        puts "Repository root path: #{@root_path}"
        raise RepositoryNotFoundError, "Could not find repository root" unless @root_path && valid?
      end

      # Core paths relative to repository root
      def fixtures_path
        @root_path / 'test' / 'fixtures' / 'pngsuite'
      end

      def expectations_path
        @root_path / 'test' / 'expectations' / 'pngsuite'
      end

      def unity_path
        @root_path / 'test' / 'test'
      end

      def test_suite_file
        unity_path / 'test_pngcheck_suite.c'
      end

      # Pngcheck executable candidates
      def pngcheck_candidates
        [
          @root_path / 'pngcheck',
          @root_path / 'build' / 'pngcheck',
          @root_path / 'pngcheck.exe',
          @root_path / 'build' / 'Release' / 'pngcheck.exe'
        ]
      end

      # Ensure directories exist
      def ensure_directories!
        [fixtures_path, expectations_path, unity_path].each do |path|
          path.mkpath unless path.exist?
        end
      end

      # Check if we're in a valid repository
      def valid?
        @root_path && (@root_path / 'pngcheck.c').exist?
      end

    end
  end
end
