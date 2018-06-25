from jinja2 import Environment, FileSystemLoader
import os
import json

# Capture our current directory
THIS_DIR = os.path.dirname(os.path.abspath(__file__))

def render_template():
    context = json.loads(open(THIS_DIR+'/templates/gen_syscall_header_context.json', 'r').read())
    
    # Create the jinja2 environment.
    # Notice the use of trim_blocks, which greatly helps control whitespace.
    j2_env = Environment(loader=FileSystemLoader(THIS_DIR),
                         trim_blocks=True)
    print j2_env.get_template('templates/gen_syscall_header.jinja2').render(context=context)

if __name__ == '__main__':
    render_template()


