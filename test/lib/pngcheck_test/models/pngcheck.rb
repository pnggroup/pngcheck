# frozen_string_literal: true

require 'open3'

module PngcheckTest
  module Models
    class PngCheck
      class ExecutableNotFoundError < StandardError; end

      attr_reader :repository, :executable_path

      def initialize(repository)
        @repository = repository
        @executable_path = find_executable
        raise ExecutableNotFoundError, executable_not_found_message unless @executable_path
      end

      # Run pngcheck on a file and return output
      def run(png_file)
        stdout, stderr, status = Open3.capture3(@executable_path.to_s, png_file.to_s)

        # Combine stdout and stderr for complete output
        output = stdout + stderr

        {
          output: output,
          exit_code: status.exitstatus,
          success: status.success? || status.exitstatus == 1 # pngcheck returns 1 for invalid files
        }
      end

      # Check if executable exists and is working
      def valid?
        return false unless @executable_path&.exist?

        # Try to run pngcheck with no arguments to verify it works
        # pngcheck will try to read from stdin and exit with code 2, which is expected
        begin
          _, _, status = Open3.capture3(@executable_path.to_s)
          # pngcheck exits with 2 when no arguments provided (tries to read stdin), which is normal
          status.exitstatus == 2
        rescue
          false
        end
      end

      # Get version information
      def version
        return nil unless valid?

        begin
          # Use an invalid option to trigger help output which includes version
          stdout, stderr, _ = Open3.capture3(@executable_path.to_s, '--invalid-option')
          output = stdout + stderr
          # Extract version from help output - note the comma after version number
          if output =~ /PNGcheck, version ([\d.]+),/
            $1
          else
            'unknown'
          end
        rescue
          nil
        end
      end

      private

      def find_executable
        # Check environment variable first
        env_path = ENV['PNGCHECK_EXECUTABLE']
        if env_path && Pathname.new(env_path).exist?
          return Pathname.new(env_path)
        end

        # Try repository-relative candidates
        @repository.pngcheck_candidates.each do |candidate|
          return candidate if candidate.exist?
        end

        # Try system PATH
        system_executable = find_in_system_path
        return Pathname.new(system_executable) if system_executable

        nil
      end

      def find_in_system_path
        begin
          stdout, _, status = Open3.capture3('which', 'pngcheck')
          return stdout.strip if status.success? && !stdout.strip.empty?
        rescue
          # which command not available, try where on Windows
          begin
            stdout, _, status = Open3.capture3('where', 'pngcheck')
            return stdout.lines.first&.strip if status.success? && !stdout.strip.empty?
          rescue
            # Neither which nor where available
          end
        end

        nil
      end

      def executable_not_found_message
        candidates = @repository.pngcheck_candidates.map(&:to_s)

        <<~MESSAGE
          pngcheck executable not found!

          Please build pngcheck first or set PNGCHECK_EXECUTABLE environment variable.

          Searched locations:
          #{candidates.map { |c| "  - #{c}" }.join("\n")}

          To build pngcheck:
            make

          Or set environment variable:
            export PNGCHECK_EXECUTABLE=/path/to/pngcheck
        MESSAGE
      end
    end
  end
end
