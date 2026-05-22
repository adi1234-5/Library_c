from flask import Flask

from auth import current_user
from config import Config
from database import close_db, ensure_database
from routes import register_routes


def create_app():
    app = Flask(
        __name__,
        template_folder=str(Config.TEMPLATES_PATH),
        static_folder=str(Config.STATIC_PATH),
        static_url_path="/assets",
    )
    app.config.from_object(Config)

    app.teardown_appcontext(close_db)

    @app.context_processor
    def inject_user():
        return {"current_user": current_user()}

    register_routes(app)

    with app.app_context():
        ensure_database()

    return app
