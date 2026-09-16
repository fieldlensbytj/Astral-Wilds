using UnityEditor;

namespace AstralWilds.EditorTools
{
    /// <summary>
    /// Ensures anything under Assets/Resources/Portraits imports as a UI sprite
    /// automatically, so Resources.Load<Sprite> works without manual per-file setup.
    /// </summary>
    public sealed class AstralPortraitImporter : AssetPostprocessor
    {
        private void OnPreprocessTexture()
        {
            if (!assetPath.Replace('\\', '/').Contains("Assets/Resources/Portraits/"))
                return;

            var importer = (TextureImporter)assetImporter;
            importer.textureType = TextureImporterType.Sprite;
            importer.spriteImportMode = SpriteImportMode.Single;
            importer.mipmapEnabled = false;
            importer.alphaIsTransparency = false;
        }
    }
}
