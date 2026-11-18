from glob import glob

from setuptools import find_packages, setup

package_name = 'uav_comms_swarm_agent'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='efe',
    maintainer_email='efecekic0@gmail.com',
    description='Swarm agent(s) leaderless coordination libraries.',
    license='AGPL-3.0',
    entry_points={
        'console_scripts': [
            'swarm_agent = uav_comms_swarm_agent.swarm_agent:main',
        ],
    },
)
